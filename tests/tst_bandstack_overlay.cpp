// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_bandstack_overlay.cpp  (NereusSDR)
// =================================================================
//
// The spectrum ticks for a band's saved entries. At the widget: what an equal
// push costs, what the ticks draw, and which press recalls, tunes, drags or
// opens the entry's menu. On a real MainWindow offscreen with a fake access:
// the switch, the pan's slice, on air, an older Core, and a real click and
// right-click, which go in at the surface that holds the pan (the pan is native
// on the Mac and Windows but not on Linux).
//
// QRhi does not run offscreen, so the GPU bake's call site is checked by eye on
// a native build; both paint paths call the function painted here.
//
// NEREUS_BANDSTACK_CAPTURE_DIR, when set, saves the ticks there, to read by eye.
//
// Design: docs/architecture/2026-11-05-band-stack-design.md, section 7, V6, V7,
// V11, V17.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-06 - New for NereusSDR by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <QAction>
#include <QApplication>
#include <QContextMenuEvent>
#include <QDir>
#include <QImage>
#include <QJsonArray>
#include <QJsonObject>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QScopeGuard>
#include <QTimer>
#include <QToolTip>
#include <QWindow>

#include <functional>
#include <memory>

#include "core/AppSettings.h"
#include "core/RadioDiscovery.h"
#include "fakes/FakeBandStackAccess.h"
#include "gui/MainWindow.h"
#include "gui/PanadapterApplet.h"
#include "gui/PanadapterStack.h"
#include "gui/SpectrumWidget.h"
#include "gui/bandstack/BandStackPanel.h"
#include "gui/bandstack/BandStackPopup.h"
#include "gui/bandstack/BandStackRowList.h"
#include "gui/widgets/VfoWidget.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

// ── At the widget ────────────────────────────────────────────────────────────
constexpr int kW = 800;
constexpr int kH = 400;
constexpr double kCenterHz = 14200000.0;
constexpr double kBandwidthHz = 80000.0;  // 100 Hz per pixel
constexpr int kSpecY = 60;                // under the labels, inside the spectrum
constexpr int kWaterfallY = 350;
constexpr int kTickX = 300;
constexpr double kTickHz = kCenterHz - kBandwidthHz / 2.0 + kTickX * 100.0;
const QColor kBackground(8, 8, 10);
const QColor kLockColour(0xc9, 0xa2, 0x4a);

BandStackTick tick(const QString& id, double hz, bool active = false, bool locked = false)
{
    BandStackTick t;
    t.entryId = id;
    t.hz = hz;
    t.label = QStringLiteral("14.%1 USB").arg(id);
    t.tip = QStringLiteral("<b>%1</b>").arg(id);
    t.active = active;
    t.locked = locked;
    t.colour = SpectrumWidget::bandStackTickColour(active);
    t.filterLowHz = 150;
    t.filterHighHz = 2850;
    return t;
}

void sendMouse(QWidget* w, QEvent::Type type, QPoint pos, Qt::MouseButton button,
               Qt::MouseButtons buttons)
{
    QMouseEvent me(type, QPointF(pos), w->mapToGlobal(QPointF(pos)), button, buttons,
                   Qt::NoModifier);
    QApplication::sendEvent(w, &me);
}

void press(QWidget* w, int x, int y = kSpecY)
{
    sendMouse(w, QEvent::MouseButtonPress, QPoint(x, y), Qt::LeftButton, Qt::LeftButton);
}

void dragTo(QWidget* w, int x, int y = kSpecY)
{
    sendMouse(w, QEvent::MouseMove, QPoint(x, y), Qt::NoButton, Qt::LeftButton);
}

void release(QWidget* w, int x, int y = kSpecY)
{
    sendMouse(w, QEvent::MouseButtonRelease, QPoint(x, y), Qt::LeftButton, Qt::NoButton);
}

void click(QWidget* w, int x, int y = kSpecY)
{
    press(w, x, y);
    release(w, x, y);
}

void hover(QWidget* w, int x, int y = kSpecY)
{
    sendMouse(w, QEvent::MouseMove, QPoint(x, y), Qt::NoButton, Qt::NoButton);
}

struct Pan {
    SpectrumWidget sw;
    Pan()
    {
        sw.resize(kW, kH);
        sw.setDbmScaleVisible(false);
        sw.setFrequencyRange(kCenterHz, kBandwidthHz);
        sw.setConnectionState(ConnectionState::Connected);
        sw.setStepSize(100);
    }
    VfoWidget* receiver(int slice, double hz, int low, int high, bool selected)
    {
        VfoWidget* flag = sw.addVfoWidget(slice);
        flag->setFrequency(hz);
        flag->setFilter(low, high);
        flag->setStepHz(100);
        flag->setActiveSlice(selected);
        return flag;
    }
    QImage paint()
    {
        QImage image(kW, kH, QImage::Format_ARGB32_Premultiplied);
        image.fill(kBackground);
        QPainter painter(&image);
        sw.drawBandStackTicksForTest(painter, QRect(0, 0, kW, 300));
        painter.end();
        return image;
    }
};

bool hasColour(const QImage& image, const QColor& colour)
{
    const QRgb wanted = colour.rgba();
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (image.pixel(x, y) == wanted) {
                return true;
            }
        }
    }
    return false;
}

void settle()
{
    for (int pass = 0; pass < 8; ++pass) {
        QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
    }
}

// ── On a window ──────────────────────────────────────────────────────────────
constexpr int kSliceA = 0;
constexpr int kSliceB = 1;
constexpr double kSliceAHz = 14050000.0;
constexpr double kViewCenterHz = 14100000.0;
constexpr double kViewSpanHz = 192000.0;
constexpr qint64 kFt8Hz = 14150000;
constexpr qint64 kNetHz = 14180000;

QJsonObject entry(const QString& id, const QString& band, qint64 hz, const QString& mode,
                  bool locked = false, const QString& name = {})
{
    return QJsonObject{{QStringLiteral("id"), id},
                       {QStringLiteral("band"), band},
                       {QStringLiteral("frequencyHz"), hz},
                       {QStringLiteral("mode"), mode},
                       {QStringLiteral("filterLowHz"), 100},
                       {QStringLiteral("filterHighHz"), 2800},
                       {QStringLiteral("centerHz"), hz},
                       {QStringLiteral("spanHz"), 96000},
                       {QStringLiteral("locked"), locked},
                       {QStringLiteral("seeded"), false},
                       {QStringLiteral("name"), name},
                       {QStringLiteral("lastUsedMs"), 0}};
}

// Slice A's place on 20m is the first entry and Slice B's the second.
QJsonObject document()
{
    const QJsonObject on20m{{QStringLiteral("order"),
                             QJsonArray{QStringLiteral("net"), QStringLiteral("ft8")}},
                            {QStringLiteral("returnMode"), QStringLiteral("current")},
                            {QStringLiteral("specificId"), QString()}};
    return QJsonObject{
        {QStringLiteral("schema"), 1},
        {QStringLiteral("region"), QStringLiteral("United States")},
        {QStringLiteral("options"),
         QJsonObject{{QStringLiteral("dupeProtection"), true},
                     {QStringLiteral("expiryMinutes"), 0},
                     {QStringLiteral("dwellSeconds"), 0}}},
        {QStringLiteral("bands"), QJsonObject{{QStringLiteral("20m"), on20m}}},
        {QStringLiteral("entries"),
         QJsonArray{entry(QStringLiteral("ft8"), QStringLiteral("20m"), kFt8Hz,
                          QStringLiteral("USB")),
                    entry(QStringLiteral("net"), QStringLiteral("20m"), kNetHz,
                          QStringLiteral("USB"), true, QStringLiteral("Maritime <net>"))}},
        {QStringLiteral("places"),
         QJsonObject{{QStringLiteral("A"), QJsonObject{{QStringLiteral("20m"), QStringLiteral("net")}}},
                     {QStringLiteral("B"), QJsonObject{{QStringLiteral("20m"), QStringLiteral("ft8")}}}}},
        {QStringLiteral("lastExpiry"),
         QJsonObject{{QStringLiteral("batchId"), QString()}, {QStringLiteral("count"), 0}}}};
}

const BandStackTick* tickById(const SpectrumWidget& sw, const QString& id)
{
    for (const BandStackTick& t : sw.bandStackTicks()) {
        if (t.entryId == id) {
            return &t;
        }
    }
    return nullptr;
}

// Runs `act`, which opens a menu and waits in it. Reads the menu, closes it, then
// triggers `choose` if any, as a person's choice arrives after the menu closes.
QStringList throughMenu(const std::function<void()>& act, const QString& choose = {})
{
    QStringList seen;
    QTimer timer;
    timer.setInterval(10);
    QObject::connect(&timer, &QTimer::timeout, &timer, [&] {
        auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
        if (!menu) {
            return;
        }
        timer.stop();
        QAction* chosen = nullptr;
        for (QAction* action : menu->actions()) {
            if (action->isSeparator()) {
                continue;
            }
            const QString text = action->text().section(QLatin1Char('\t'), 0, 0);
            seen << text;
            if (text == choose) {
                chosen = action;
            }
        }
        menu->close();
        if (chosen) {
            chosen->trigger();
        }
    });
    timer.start();
    act();
    return seen;
}

struct WindowFixture {
    std::unique_ptr<MainWindow> window;
    RadioModel* model{nullptr};
    BandStackPanel* panel{nullptr};
    PanadapterStack* stack{nullptr};
    PanadapterApplet* applet{nullptr};
    SpectrumWidget* sw{nullptr};
    FakeBandStackAccess fake;

    bool build()
    {
        AppSettings::instance().remove(QString::fromLatin1(BandStackPanel::kShowOnSpectrumKey));
        window = std::make_unique<MainWindow>(RemoteStationOptions{}, nullptr,
                                              MainWindow::ConnectionStartup::Deferred);
        model = window->radioModel();
        model->setBoardForTest(HPSDRHW::Saturn);
        model->configureStreamPool(5, 5, 192000);
        while (model->slices().size() < 2) {
            if (model->addSlice() < 0) {
                return false;
            }
        }
        window->resize(1400, 860);
        window->show();
        settle();
        panel = window->findChild<BandStackPanel*>();
        stack = window->findChild<PanadapterStack*>();
        applet = stack ? stack->allApplets().value(0) : nullptr;
        sw = applet ? applet->spectrumWidget() : nullptr;
        if (!panel || !sw) {
            return false;
        }
        window->setBandStackAccessForTest(&fake);
        reset();
        return true;
    }

    void reset()
    {
        if (auto* popup = window->findChild<BandStackPopup*>()) {
            popup->close();
        }
        panel->setShowOnSpectrum(false);
        fake.setState(BandStackAccess::State::Ready);
        fake.setDocument(document());
        applet->setActiveSliceIndex(kSliceA);
        model->sliceById(kSliceA)->setFrequency(kSliceAHz);
        model->sliceById(kSliceB)->setFrequency(7074000.0);
        emit model->transmittingChanged(false);
        settle();
        sw->setConnectionState(ConnectionState::Connected);
        // With no dBm strip, the frequency axis runs over the pan's whole width.
        sw->setDbmScaleVisible(false);
        sw->setFrequencyRange(kViewCenterHz, kViewSpanHz);
        settle();
        fake.calls.clear();
    }

    // The pan moves to its slice when the slice changes; the ticks stay in view.
    void useSlice(int sliceId)
    {
        applet->setActiveSliceIndex(sliceId);
        settle();
        sw->setFrequencyRange(kViewCenterHz, kViewSpanHz);
        settle();
    }

    // Where a tick is, on the surface that holds the pan, with nothing over it.
    bool clickPoint(qint64 hz, QWindow*& surfaceWindow, QPoint& at) const
    {
        const double low = sw->centerFrequency() - sw->bandwidth() / 2.0;
        const QPoint local(static_cast<int>((static_cast<double>(hz) - low) / sw->bandwidth()
                                            * sw->width()),
                           kSpecY);
        QWidget* surface = sw->nativeParentWidget();
        if (!surface || !surface->windowHandle()) {
            return false;
        }
        surfaceWindow = surface->windowHandle();
        at = sw->mapTo(surface, local);
        return window->childAt(sw->mapTo(window.get(), local)) == sw;
    }

    ~WindowFixture()
    {
        if (window) {
            window->setBandStackAccessForTest(model->bandStack());
        }
    }
};

QAction* actionNamed(QMenu& menu, const QString& text)
{
    for (QAction* action : menu.actions()) {
        if (action->text() == text) {
            return action;
        }
    }
    return nullptr;
}

} // namespace

class TstBandStackOverlay : public QObject {
    Q_OBJECT

    std::unique_ptr<WindowFixture> m_fixture;

    bool buildFixture()
    {
        m_fixture.reset();
        m_fixture = std::make_unique<WindowFixture>();
        return m_fixture->build();
    }

private slots:
    // ── What a push costs ────────────────────────────────────────────────────
    void anEqualPushDoesNotDirtyTheOverlay()
    {
#ifndef NEREUS_GPU_SPECTRUM
        QSKIP("No cached overlay on a CPU-only build.");
#endif
        Pan p;
        const QVector<BandStackTick> ticks{tick(QStringLiteral("a"), kTickHz),
                                           tick(QStringLiteral("b"), kTickHz + 5000.0)};
        p.sw.setBandStackTicks(ticks);
        QVERIFY(p.sw.overlayStaticDirtyForTest());

        p.sw.clearOverlayStaticDirtyForTest();
        p.sw.setBandStackTicks(ticks);
        QVERIFY(!p.sw.overlayStaticDirtyForTest());
        // Equal in another order too: a document is free to list them so.
        p.sw.setBandStackTicks({ticks.at(1), ticks.at(0)});
        QVERIFY(!p.sw.overlayStaticDirtyForTest());
        p.sw.setBandStackTicksHidden(false);
        QVERIFY(!p.sw.overlayStaticDirtyForTest());

        p.sw.setBandStackTicks({tick(QStringLiteral("a"), kTickHz, true),
                                tick(QStringLiteral("b"), kTickHz + 5000.0)});
        QVERIFY(p.sw.overlayStaticDirtyForTest());

        p.sw.clearOverlayStaticDirtyForTest();
        p.sw.setBandStackTicksHidden(true);
        QVERIFY(p.sw.overlayStaticDirtyForTest());

        p.sw.clearOverlayStaticDirtyForTest();
        p.sw.setBandStackTicksHidden(false);
        QVERIFY(p.sw.overlayStaticDirtyForTest());
    }

    void movingTheRangeRedrawsTheTicks()
    {
#ifndef NEREUS_GPU_SPECTRUM
        QSKIP("No cached overlay on a CPU-only build.");
#endif
        Pan p;
        p.sw.setBandStackTicks({tick(QStringLiteral("a"), kTickHz)});
        p.sw.clearOverlayStaticDirtyForTest();
        p.sw.setFrequencyRange(kCenterHz + 10000.0, kBandwidthHz);
        QVERIFY(p.sw.overlayStaticDirtyForTest());
        // 100 Hz per pixel, so 10 kHz up moves the tick 100 pixels left.
        const QImage image = p.paint();
        QCOMPARE(image.pixelColor(kTickX - 100, 10), SpectrumWidget::bandStackTickColour(false));
        QCOMPARE(image.pixelColor(kTickX, 10), kBackground);
    }

    // ── What it draws ────────────────────────────────────────────────────────
    void savedActiveAndLockedAreDistinct()
    {
        const QColor saved(0x6e, 0xa5, 0xd8);
        const QColor active(0xd8, 0xa5, 0x5f);
        QCOMPARE(SpectrumWidget::bandStackTickColour(false), saved);
        QCOMPARE(SpectrumWidget::bandStackTickColour(true), active);

        Pan p;
        p.sw.setBandStackTicks({tick(QStringLiteral("a"), kTickHz)});
        const QImage plain = p.paint();
        QCOMPARE(plain.pixelColor(kTickX, 10), saved);
        QVERIFY(!hasColour(plain, kLockColour));

        p.sw.setBandStackTicks({tick(QStringLiteral("a"), kTickHz, true)});
        const QImage gold = p.paint();
        QCOMPARE(gold.pixelColor(kTickX, 10), active);
        QVERIFY(!hasColour(gold, kLockColour));
        // The place's line is stronger than a saved one's.
        const int plainLine = plain.pixelColor(kTickX, 200).blue();
        const int goldLine = gold.pixelColor(kTickX, 200).red();
        QVERIFY2(goldLine > plainLine, qPrintable(QStringLiteral("%1 %2").arg(goldLine).arg(plainLine)));

        p.sw.setBandStackTicks({tick(QStringLiteral("a"), kTickHz, false, true)});
        const QImage locked = p.paint();
        QCOMPARE(locked.pixelColor(kTickX, 10), saved);
        QVERIFY(hasColour(locked, kLockColour));

        const QString directory = qEnvironmentVariable("NEREUS_BANDSTACK_CAPTURE_DIR");
        if (!directory.isEmpty()) {
            p.sw.setBandStackTicks({tick(QStringLiteral("185"), kTickHz),
                                    tick(QStringLiteral("190"), kTickHz + 3000.0, true),
                                    tick(QStringLiteral("195"), kTickHz + 6000.0, false, true),
                                    tick(QStringLiteral("200"), kTickHz + 20000.0)});
            hover(&p.sw, kTickX + 200);
            QVERIFY(p.paint().save(QDir(directory).filePath(QStringLiteral("overlay-ticks.png"))));
        }
    }

    void crowdedLabelsStepDownARow()
    {
        Pan p;
        // The second label row's text, right of both ticks' lines.
        const auto secondRowHasText = [](const QImage& image) {
            for (int y = 31; y <= 40; ++y) {
                for (int x = kTickX + 25; x < kTickX + 90; ++x) {
                    if (image.pixelColor(x, y) != kBackground) {
                        return true;
                    }
                }
            }
            return false;
        };
        p.sw.setBandStackTicks({tick(QStringLiteral("a"), kTickHz)});
        QVERIFY(!secondRowHasText(p.paint()));
        p.sw.setBandStackTicks({tick(QStringLiteral("a"), kTickHz),
                                tick(QStringLiteral("b"), kTickHz + 2000.0)});
        QVERIFY(secondRowHasText(p.paint()));
    }

    void aTickOutOfViewIsNeitherDrawnNorHit()
    {
        Pan p;
        const double outOfView = kCenterHz + kBandwidthHz;
        p.sw.setBandStackTicks({tick(QStringLiteral("a"), outOfView)});
        QVERIFY(!hasColour(p.paint(), SpectrumWidget::bandStackTickColour(false)));
        QSignalSpy clicked(&p.sw, &SpectrumWidget::bandStackTickClicked);
        click(&p.sw, kW - 1);
        QCOMPARE(clicked.count(), 0);
    }

    void hiddenTicksNeitherDrawNorTakeAClick()
    {
        Pan p;
        p.sw.setBandStackTicks({tick(QStringLiteral("a"), kTickHz)});
        p.sw.setBandStackTicksHidden(true);
        QVERIFY(!hasColour(p.paint(), SpectrumWidget::bandStackTickColour(false)));

        QSignalSpy clicked(&p.sw, &SpectrumWidget::bandStackTickClicked);
        QSignalSpy tuned(&p.sw, &SpectrumWidget::frequencyClicked);
        click(&p.sw, kTickX);
        QCOMPARE(clicked.count(), 0);
        QCOMPARE(tuned.count(), 1);
        hover(&p.sw, kTickX);
        QVERIFY(p.sw.hoveredBandStackTick().isEmpty());
    }

    // ── A click ──────────────────────────────────────────────────────────────
    void aClickWithin4PxRecallsAnd5PxTunes_data()
    {
        QTest::addColumn<int>("offset");
        QTest::addColumn<bool>("recalls");
        QTest::newRow("on it") << 0 << true;
        QTest::newRow("4 px below") << -4 << true;
        QTest::newRow("4 px above") << 4 << true;
        QTest::newRow("5 px below") << -5 << false;
        QTest::newRow("5 px above") << 5 << false;
    }

    void aClickWithin4PxRecallsAnd5PxTunes()
    {
        QFETCH(int, offset);
        QFETCH(bool, recalls);
        Pan p;
        p.sw.setBandStackTicks({tick(QStringLiteral("a"), kTickHz)});
        QSignalSpy clicked(&p.sw, &SpectrumWidget::bandStackTickClicked);
        QSignalSpy tuned(&p.sw, &SpectrumWidget::frequencyClicked);
        click(&p.sw, kTickX + offset);
        QCOMPARE(clicked.count(), recalls ? 1 : 0);
        QCOMPARE(tuned.count(), recalls ? 0 : 1);
        if (recalls) {
            QCOMPARE(clicked.first().first().toString(), QStringLiteral("a"));
        }
    }

    void theNearestTickTakesTheClick()
    {
        Pan p;
        p.sw.setBandStackTicks({tick(QStringLiteral("a"), kTickHz),
                                tick(QStringLiteral("b"), kTickHz + 500.0)});
        QSignalSpy clicked(&p.sw, &SpectrumWidget::bandStackTickClicked);
        click(&p.sw, kTickX + 4);
        QCOMPARE(clicked.count(), 1);
        QCOMPARE(clicked.first().first().toString(), QStringLiteral("b"));
    }

    void onlyTheSpectrumAreaTakesAClick()
    {
        Pan p;
        p.sw.setBandStackTicks({tick(QStringLiteral("a"), kTickHz)});
        QSignalSpy clicked(&p.sw, &SpectrumWidget::bandStackTickClicked);
        QSignalSpy tuned(&p.sw, &SpectrumWidget::frequencyClicked);
        click(&p.sw, kTickX, kWaterfallY);
        QCOMPARE(clicked.count(), 0);
        QCOMPARE(tuned.count(), 1);
    }

    void aDragFromATickPansAsAnyOtherDoes()
    {
        Pan p;
        p.sw.setBandStackTicks({tick(QStringLiteral("a"), kTickHz)});
        QSignalSpy clicked(&p.sw, &SpectrumWidget::bandStackTickClicked);
        QSignalSpy panned(&p.sw, &SpectrumWidget::centerChanged);
        press(&p.sw, kTickX);
        dragTo(&p.sw, kTickX + 40);
        release(&p.sw, kTickX + 40);
        QCOMPARE(clicked.count(), 0);
        QVERIFY(panned.count() > 0);
    }

    // V17: the marker line and the filter edges keep their drags.
    void aSliceMarkerLineWinsOverATickUnderIt()
    {
        Pan p;
        VfoWidget* a = p.receiver(0, kTickHz, 0, 400, true);
        p.sw.setBandStackTicks({tick(QStringLiteral("a"), kTickHz)});
        QSignalSpy clicked(&p.sw, &SpectrumWidget::bandStackTickClicked);
        QSignalSpy tuned(a, &VfoWidget::frequencyChanged);

        click(&p.sw, kTickX);
        QCOMPARE(clicked.count(), 0);

        press(&p.sw, kTickX);
        dragTo(&p.sw, kTickX + 20);
        release(&p.sw, kTickX + 20);
        QCOMPARE(clicked.count(), 0);
        QCOMPARE(tuned.count(), 1);

        hover(&p.sw, kTickX + 20);
        QVERIFY(p.sw.hoveredBandStackTick().isEmpty());
    }

    void aFilterEdgeWinsOverATickUnderIt()
    {
        Pan p;
        constexpr int kEdgeOffsetPx = 40;  // well off the marker line's 8 px
        VfoWidget* a = p.receiver(0, kTickHz - kEdgeOffsetPx * 100.0, kEdgeOffsetPx * 100, 8000, true);
        p.sw.setBandStackTicks({tick(QStringLiteral("a"), kTickHz)});
        QSignalSpy clicked(&p.sw, &SpectrumWidget::bandStackTickClicked);
        QSignalSpy edged(a, &VfoWidget::filterChanged);
        press(&p.sw, kTickX);
        dragTo(&p.sw, kTickX + 10);
        release(&p.sw, kTickX + 10);
        QCOMPARE(clicked.count(), 0);
        QVERIFY(edged.count() > 0);
    }

    // Inside a passband, away from the line: a click recalls, a drag still slides.
    void aTickInsideAPassbandRecallsOnAClickAndSlidesOnADrag()
    {
        constexpr int kOffsetPx = 40;
        constexpr int kLowHz = -6000;
        constexpr int kHighHz = 6000;
        const double receiverHz = kTickHz - kOffsetPx * 100.0;
        Pan p;
        VfoWidget* a = p.receiver(0, receiverHz, kLowHz, kHighHz, true);
        p.sw.setBandStackTicks({tick(QStringLiteral("a"), kTickHz)});
        QSignalSpy clicked(&p.sw, &SpectrumWidget::bandStackTickClicked);
        QSignalSpy tuned(a, &VfoWidget::frequencyChanged);

        click(&p.sw, kTickX);
        QCOMPARE(clicked.count(), 1);
        QCOMPARE(tuned.count(), 0);

        // A wobble inside a click's travel is still the click, and tunes nothing.
        press(&p.sw, kTickX);
        dragTo(&p.sw, kTickX + 3);
        release(&p.sw, kTickX + 3);
        QCOMPARE(clicked.count(), 2);
        QCOMPARE(tuned.count(), 0);

        press(&p.sw, kTickX);
        dragTo(&p.sw, kTickX + 30);
        release(&p.sw, kTickX + 30);
        QCOMPARE(clicked.count(), 2);
        QCOMPARE(tuned.count(), 1);
        QCOMPARE(tuned.first().first().toDouble(), receiverHz + 30 * 100.0);
    }

    void aRecallInAnotherReceiversPassbandSelectsNothing()
    {
        constexpr int kOffsetPx = 40;
        const double receiverHz = kTickHz - kOffsetPx * 100.0;
        Pan p;
        p.receiver(0, kCenterHz + 30000.0, -3000, 3000, true);
        VfoWidget* other = p.receiver(1, receiverHz, -6000, 6000, false);
        p.sw.setBandStackTicks({tick(QStringLiteral("a"), kTickHz)});
        QSignalSpy clicked(&p.sw, &SpectrumWidget::bandStackTickClicked);
        QSignalSpy selected(other, &VfoWidget::sliceActivationRequested);

        click(&p.sw, kTickX);
        QCOMPARE(clicked.count(), 1);
        QCOMPARE(selected.count(), 0);

        // Becoming a drag selects it, as a drag on that receiver always has.
        press(&p.sw, kTickX);
        dragTo(&p.sw, kTickX + 30);
        release(&p.sw, kTickX + 30);
        QCOMPARE(clicked.count(), 1);
        QCOMPARE(selected.count(), 1);
    }

    // ── Hover ────────────────────────────────────────────────────────────────
    void hoverShowsTheTipAndLeavingClearsIt()
    {
        Pan p;
        p.sw.show();
        p.sw.setBandStackTicks({tick(QStringLiteral("a"), kTickHz)});
        hover(&p.sw, kTickX + 4);
        QCOMPARE(p.sw.hoveredBandStackTick(), QStringLiteral("a"));
        QCOMPARE(QToolTip::text(), QStringLiteral("<b>a</b>"));
        QCOMPARE(p.sw.cursor().shape(), Qt::PointingHandCursor);

        hover(&p.sw, kTickX + 5);
        QVERIFY(p.sw.hoveredBandStackTick().isEmpty());
        QTRY_VERIFY(QToolTip::text().isEmpty());

        hover(&p.sw, kTickX);
        QCOMPARE(p.sw.hoveredBandStackTick(), QStringLiteral("a"));
        QEvent leave(QEvent::Leave);
        QApplication::sendEvent(&p.sw, &leave);
        QVERIFY(p.sw.hoveredBandStackTick().isEmpty());
    }

    // ── Right-click ──────────────────────────────────────────────────────────
    void aRightClickOnATickAsksForItsMenuAndNotThePans()
    {
        Pan p;
        p.sw.setBandStackTicks({tick(QStringLiteral("a"), kTickHz)});
        QSignalSpy context(&p.sw, &SpectrumWidget::bandStackTickContext);
        sendMouse(&p.sw, QEvent::MouseButtonPress, QPoint(kTickX + 2, kSpecY), Qt::RightButton,
                  Qt::RightButton);
        QCOMPARE(context.count(), 1);
        QCOMPARE(context.first().first().toString(), QStringLiteral("a"));
        QCOMPARE(context.first().last().toPoint(), p.sw.mapToGlobal(QPoint(kTickX + 2, kSpecY)));
        sendMouse(&p.sw, QEvent::MouseButtonRelease, QPoint(kTickX + 2, kSpecY), Qt::RightButton,
                  Qt::NoButton);

        // The event Qt sends after the press stays away from the applet's menu,
        // on the tick and when the menu's Delete has taken the tick away.
        p.sw.setBandStackTicks({});
        QContextMenuEvent afterDelete(QContextMenuEvent::Mouse, QPoint(kTickX + 2, kSpecY));
        afterDelete.ignore();
        QApplication::sendEvent(&p.sw, &afterDelete);
        QVERIFY(afterDelete.isAccepted());

        QContextMenuEvent elsewhere(QContextMenuEvent::Mouse, QPoint(kTickX + 2, kSpecY));
        elsewhere.accept();
        QApplication::sendEvent(&p.sw, &elsewhere);
        QVERIFY(!elsewhere.isAccepted());
    }

    // ── On a window ──────────────────────────────────────────────────────────
    void init()
    {
        if (!QByteArray(QTest::currentTestFunction()).startsWith("window")) {
            return;
        }
        if (!m_fixture) {
            QVERIFY(buildFixture());
        } else {
            m_fixture->reset();
        }
    }

    void cleanup()
    {
        if (QTest::currentTestFailed()) {
            m_fixture.reset();
        }
    }

    void cleanupTestCase() { m_fixture.reset(); }

    void windowTicksAreOffUntilThePanelMenuTurnsThemOn()
    {
        WindowFixture& f = *m_fixture;
        QVERIFY(!f.panel->showOnSpectrum());
        QVERIFY(f.sw->bandStackTicks().isEmpty());

        QMenu menu;
        f.panel->fillPanelMenu(menu);
        QAction* item = actionNamed(menu, QStringLiteral("Show on the spectrum"));
        QVERIFY(item);
        QVERIFY(item->isCheckable());
        QVERIFY(!item->isChecked());
        item->trigger();

        QCOMPARE(f.sw->bandStackTicks().size(), 2);
        QCOMPARE(AppSettings::instance()
                     .value(QString::fromLatin1(BandStackPanel::kShowOnSpectrumKey)).toString(),
                 QStringLiteral("True"));

        f.panel->setShowOnSpectrum(false);
        QVERIFY(f.sw->bandStackTicks().isEmpty());
        QCOMPARE(AppSettings::instance()
                     .value(QString::fromLatin1(BandStackPanel::kShowOnSpectrumKey)).toString(),
                 QStringLiteral("False"));
    }

    void windowTicksSayWhatTheEntryIs()
    {
        WindowFixture& f = *m_fixture;
        f.panel->setShowOnSpectrum(true);

        const BandStackTick* ft8 = tickById(*f.sw, QStringLiteral("ft8"));
        const BandStackTick* net = tickById(*f.sw, QStringLiteral("net"));
        QVERIFY(ft8 && net);
        QCOMPARE(ft8->hz, static_cast<double>(kFt8Hz));
        QCOMPARE(ft8->label, QStringLiteral("14.150 USB"));
        // The band's own order, not the document's list of entries.
        QCOMPARE(ft8->tip, QStringLiteral("<b>14.150 MHz</b><br>USB, 2.7K<br>2 of 2 on 20m"));
        QCOMPARE(net->tip, QStringLiteral("<b>14.180 MHz</b><br>USB, 2.7K<br>1 of 2 on 20m"
                                          "<br><i>Maritime &lt;net&gt;</i><br>Locked"));
        QVERIFY(net->locked);
        QVERIFY(!ft8->locked);
        QCOMPARE(ft8->filterLowHz, 100);
        QCOMPARE(ft8->filterHighHz, 2800);
    }

    void windowTheHighlightFollowsThePansSlice()
    {
        WindowFixture& f = *m_fixture;
        f.panel->setShowOnSpectrum(true);
        QVERIFY(tickById(*f.sw, QStringLiteral("net"))->active);
        QVERIFY(!tickById(*f.sw, QStringLiteral("ft8"))->active);
        QCOMPARE(tickById(*f.sw, QStringLiteral("net"))->colour,
                 SpectrumWidget::bandStackTickColour(true));

        f.applet->setActiveSliceIndex(kSliceB);
        QVERIFY(!tickById(*f.sw, QStringLiteral("net"))->active);
        QVERIFY(tickById(*f.sw, QStringLiteral("ft8"))->active);
    }

    void windowAnOlderCoreShowsNoTicks()
    {
        WindowFixture& f = *m_fixture;
        f.panel->setShowOnSpectrum(true);
        QCOMPARE(f.sw->bandStackTicks().size(), 2);

        f.fake.setState(BandStackAccess::State::NeedsNewerCore,
                        BandStackAccess::needsNewerCoreReason());
        QVERIFY(f.sw->bandStackTicks().isEmpty());
        f.fake.setState(BandStackAccess::State::Ready);
        QCOMPARE(f.sw->bandStackTicks().size(), 2);
    }

    void windowEveryPanFollowsTheSwitchAndGoesOnAir()
    {
        WindowFixture& f = *m_fixture;
        PanadapterApplet* second = f.stack->addPanadapter(QStringLiteral("pan-overlay-test"));
        QVERIFY(second);
        const auto removeSecond = qScopeGuard(
            [&f] { f.stack->removePanadapter(QStringLiteral("pan-overlay-test")); });
        SpectrumWidget* other = second->spectrumWidget();
        QVERIFY(other);

        f.panel->setShowOnSpectrum(true);
        QCOMPARE(f.sw->bandStackTicks().size(), 2);
        QCOMPARE(other->bandStackTicks().size(), 2);

        emit f.model->transmittingChanged(true);
        QVERIFY(f.sw->bandStackTicksHidden());
        QVERIFY(other->bandStackTicksHidden());

        emit f.model->transmittingChanged(false);
        QVERIFY(!f.sw->bandStackTicksHidden());
        QVERIFY(!other->bandStackTicksHidden());

        f.panel->setShowOnSpectrum(false);
        QVERIFY(f.sw->bandStackTicks().isEmpty());
        QVERIFY(other->bandStackTicks().isEmpty());
    }

    void windowARealClickRecallsOnThePansSlice()
    {
        WindowFixture& f = *m_fixture;
        f.panel->setShowOnSpectrum(true);
        QWindow* surface = nullptr;
        QPoint at;
        QVERIFY2(f.clickPoint(kFt8Hz, surface, at), "Something of the window's covers the tick.");

        QTest::mouseClick(surface, Qt::LeftButton, Qt::NoModifier, at);
        QCOMPARE(f.fake.calls.size(), 1);
        QCOMPARE(f.fake.calls.first().verb, QStringLiteral("goTo"));
        QCOMPARE(f.fake.calls.first().sliceId, kSliceA);
        QCOMPARE(f.fake.calls.first().entryId, QStringLiteral("ft8"));
        QCOMPARE(f.fake.calls.first().view.spanHz, qRound64(f.sw->bandwidth()));

        f.fake.calls.clear();
        f.useSlice(kSliceB);
        QVERIFY(f.clickPoint(kFt8Hz, surface, at));
        QTest::mouseClick(surface, Qt::LeftButton, Qt::NoModifier, at);
        QCOMPARE(f.fake.calls.size(), 1);
        QCOMPARE(f.fake.calls.first().sliceId, kSliceB);
    }

    void windowARealClickOnAirRecallsNothing()
    {
        WindowFixture& f = *m_fixture;
        f.panel->setShowOnSpectrum(true);
        QWindow* surface = nullptr;
        QPoint at;
        QVERIFY(f.clickPoint(kFt8Hz, surface, at));
        emit f.model->transmittingChanged(true);
        QTest::mouseClick(surface, Qt::LeftButton, Qt::NoModifier, at);
        QVERIFY(f.fake.calls.isEmpty());
    }

    void windowARealRightClickOpensTheRowMenuOnThePansSlice()
    {
        WindowFixture& f = *m_fixture;
        f.panel->setShowOnSpectrum(true);
        f.useSlice(kSliceB);
        QWindow* surface = nullptr;
        QPoint at;
        QVERIFY(f.clickPoint(kFt8Hz, surface, at));

        const QStringList seen = throughMenu(
            [&] { QTest::mouseClick(surface, Qt::RightButton, Qt::NoModifier, at); },
            QStringLiteral("Go there"));
        QCOMPARE(seen, (QStringList{QStringLiteral("Go there"),
                                    QStringLiteral("Save current spot here"),
                                    QStringLiteral("Rename"), QStringLiteral("Lock"),
                                    QStringLiteral("Always return here"),
                                    QStringLiteral("Delete")}));
        QCOMPARE(f.fake.calls.size(), 1);
        QCOMPARE(f.fake.calls.first().verb, QStringLiteral("goTo"));
        QCOMPARE(f.fake.calls.first().sliceId, kSliceB);
        QCOMPARE(f.fake.calls.first().entryId, QStringLiteral("ft8"));
        // No pan menu after it.
        settle();
        QVERIFY(!QApplication::activePopupWidget());
    }

    void windowRenameFromATickOpensThePopupWithItsEditor()
    {
        WindowFixture& f = *m_fixture;
        f.panel->setShowOnSpectrum(true);
        QWindow* surface = nullptr;
        QPoint at;
        QVERIFY(f.clickPoint(kFt8Hz, surface, at));

        throughMenu([&] { QTest::mouseClick(surface, Qt::RightButton, Qt::NoModifier, at); },
                    QStringLiteral("Rename"));
        auto* popup = f.window->findChild<BandStackPopup*>();
        QVERIFY(popup);
        QTRY_VERIFY(popup->isVisible());
        QCOMPARE(popup->band(), std::optional<Band>(Band::Band20m));
        auto* editor = popup->findChild<QLineEdit*>(QStringLiteral("bandStackRowRename"));
        QVERIFY(editor);
        editor->setText(QStringLiteral("Calling"));
        QTest::keyClick(editor, Qt::Key_Return);
        QCOMPARE(f.fake.calls.size(), 1);
        QCOMPARE(f.fake.calls.first().verb, QStringLiteral("edit"));
        QCOMPARE(f.fake.calls.first().entryId, QStringLiteral("ft8"));
        QCOMPARE(f.fake.calls.first().name, std::optional<QString>(QStringLiteral("Calling")));
    }
};

QTEST_MAIN(TstBandStackOverlay)
#include "tst_bandstack_overlay.moc"
