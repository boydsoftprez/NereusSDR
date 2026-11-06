// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_bandstack_router.cpp  (NereusSDR)
// =================================================================
//
// Every band press in the desktop goes through MainWindow::pressBand. A real
// MainWindow offscreen with a fake BandStackAccess: which verb, which slice and
// which view each path sends, with the modifiers held, and the old path when the
// stack is not ready. Also STACK+ and STACK-, the four hot key entry points, and
// one toast per refusal or report. Then the popup: which slice and band each
// path opens it for, where it opens, what keeps it open and what closes it, and
// the reason it shows with no usable stack.
//
// One MainWindow serves every case, put back to a known state before each one.
// A window takes over a second to build, and the saved container each window
// adds is restored by the next, so a window per case cost 105 s on the Mac and
// ran past 120 s on Linux CI. A case that fails, or one that leaves the window
// unusable, gets a new window for the case after it.
//
// NEREUS_BANDSTACK_CAPTURE_DIR, when set, saves the popup in each look there, to
// read by eye.
//
// Design: docs/architecture/2026-11-05-band-stack-design.md, sections 5.1 and 7.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-06 - New for NereusSDR by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
//   2026-11-06 - The popup (band stack plan Task 10). J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-11-06 - One window for every case; the fixture's container no longer
//                 covers the flyout. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <QAction>
#include <QApplication>
#include <QContextMenuEvent>
#include <QDir>
#include <QGridLayout>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPushButton>
#include <QScopeGuard>
#include <QScreen>
#include <QScrollArea>
#include <QTimer>
#include <QWindow>

#include <functional>
#include <memory>

#include "core/RadioDiscovery.h"
#include "fakes/FakeBandStackAccess.h"
#include "gui/MainWindow.h"
#include "gui/SpectrumOverlayPanel.h"
#include "gui/SpectrumWidget.h"
#include "gui/bandstack/BandStackPopup.h"
#include "gui/bandstack/BandStackRowList.h"
#include "gui/containers/ContainerManager.h"
#include "gui/containers/ContainerWidget.h"
#include "gui/meters/MeterWidget.h"
#include "gui/meters/OtherButtonItem.h"
#include "gui/meters/VfoDisplayItem.h"
#include "gui/styles/AppTheme.h"
#include "gui/styles/WorkspacePreview.h"
#include "gui/widgets/StatusToast.h"
#include "models/BandDefaults.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

constexpr int kSliceA = 0;
constexpr int kSliceB = 1;
constexpr int kContainerRxSourceB = 2;
constexpr double kOn20mHz = 14'074'000.0;
constexpr double kOn40mHz = 7'074'000.0;
constexpr double kOnWwvHz = 5'000'000.0;
constexpr double kOnGenHz = 909'000.0;

void settle()
{
    for (int pass = 0; pass < 8; ++pass) {
        QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
        QCoreApplication::processEvents(QEventLoop::AllEvents);
    }
}

// The window-level key path, so QGuiApplication::keyboardModifiers() reads it as
// it reads a person holding the key.
void holdModifiers(QWindow* window, Qt::KeyboardModifiers mods)
{
    if (mods.testFlag(Qt::ControlModifier)) {
        QTest::keyPress(window, Qt::Key_Control, mods);
    } else if (mods.testFlag(Qt::ShiftModifier)) {
        QTest::keyPress(window, Qt::Key_Shift, mods);
    }
}

void releaseModifiers(QWindow* window, Qt::KeyboardModifiers mods)
{
    if (mods.testFlag(Qt::ControlModifier)) {
        QTest::keyRelease(window, Qt::Key_Control, Qt::NoModifier);
    } else if (mods.testFlag(Qt::ShiftModifier)) {
        QTest::keyRelease(window, Qt::Key_Shift, Qt::NoModifier);
    }
}

int toastCount(MainWindow& window, const QString& message)
{
    int count = 0;
    for (StatusToast* toast : window.findChildren<StatusToast*>()) {
        if (toast->message() == message) {
            ++count;
        }
    }
    return count;
}

QAction* bandMenuEntry(MainWindow& window, const QString& text)
{
    for (QMenu* menu : window.findChildren<QMenu*>()) {
        if (menu->title() != QStringLiteral("&Band")) {
            continue;
        }
        QList<QMenu*> menus{menu};
        while (!menus.isEmpty()) {
            for (QAction* entry : menus.takeFirst()->actions()) {
                if (entry->text() == text) {
                    return entry;
                }
                if (entry->menu()) {
                    menus << entry->menu();
                }
            }
        }
    }
    return nullptr;
}

QJsonObject popupEntry(const QString& id, const QString& band, qint64 hz, const QString& mode,
                       bool locked = false, const QString& name = {})
{
    return QJsonObject{{QStringLiteral("id"), id},
                       {QStringLiteral("band"), band},
                       {QStringLiteral("frequencyHz"), hz},
                       {QStringLiteral("mode"), mode},
                       {QStringLiteral("filterLowHz"), -2800},
                       {QStringLiteral("filterHighHz"), -100},
                       {QStringLiteral("centerHz"), hz},
                       {QStringLiteral("spanHz"), 96000},
                       {QStringLiteral("locked"), locked},
                       {QStringLiteral("seeded"), false},
                       {QStringLiteral("name"), name},
                       {QStringLiteral("lastUsedMs"), 0}};
}

QJsonObject popupBand(const QStringList& ids)
{
    return QJsonObject{{QStringLiteral("order"), QJsonArray::fromStringList(ids)},
                       {QStringLiteral("returnMode"), QStringLiteral("current")},
                       {QStringLiteral("specificId"), QString()}};
}

// Three spots on 40m with Slice B on the first, one on each band the flyout and
// grid paths below open, and `extra40m` more on 40m to make the list scroll.
QJsonObject popupDocument(int extra40m = 0)
{
    QStringList on40m{QStringLiteral("net"), QStringLiteral("ssb"), QStringLiteral("dx")};
    QJsonArray entries{popupEntry(QStringLiteral("net"), QStringLiteral("40m"), 7074000,
                                  QStringLiteral("LSB")),
                       popupEntry(QStringLiteral("ssb"), QStringLiteral("40m"), 7200000,
                                  QStringLiteral("LSB"), true),
                       popupEntry(QStringLiteral("dx"), QStringLiteral("40m"), 7255500,
                                  QStringLiteral("LSB"), false, QStringLiteral("Evening net")),
                       popupEntry(QStringLiteral("ft8"), QStringLiteral("20m"), 14074000,
                                  QStringLiteral("USB")),
                       popupEntry(QStringLiteral("top"), QStringLiteral("160m"), 1840000,
                                  QStringLiteral("LSB")),
                       popupEntry(QStringLiteral("time"), QStringLiteral("WWV"), 10000000,
                                  QStringLiteral("AM"))};
    for (int i = 0; i < extra40m; ++i) {
        const QString id = QStringLiteral("more%1").arg(i);
        on40m << id;
        entries.append(popupEntry(id, QStringLiteral("40m"), 7100000 + i * 1000,
                                  QStringLiteral("LSB")));
    }
    return QJsonObject{
        {QStringLiteral("schema"), 1},
        {QStringLiteral("region"), QStringLiteral("United States")},
        {QStringLiteral("options"),
         QJsonObject{{QStringLiteral("dupeProtection"), true},
                     {QStringLiteral("expiryMinutes"), 0},
                     {QStringLiteral("dwellSeconds"), 0}}},
        {QStringLiteral("bands"),
         QJsonObject{{QStringLiteral("40m"), popupBand(on40m)},
                     {QStringLiteral("20m"), popupBand({QStringLiteral("ft8")})},
                     {QStringLiteral("160m"), popupBand({QStringLiteral("top")})},
                     {QStringLiteral("WWV"), popupBand({QStringLiteral("time")})}}},
        {QStringLiteral("entries"), entries},
        {QStringLiteral("places"),
         QJsonObject{{QStringLiteral("B"),
                      QJsonObject{{QStringLiteral("40m"), QStringLiteral("net")}}}}},
        {QStringLiteral("lastExpiry"),
         QJsonObject{{QStringLiteral("batchId"), QString()}, {QStringLiteral("count"), 0}}}};
}

QPushButton* flyoutButton(SpectrumOverlayPanel& overlay, const QString& text)
{
    for (QPushButton* button : overlay.parentWidget()->findChildren<QPushButton*>()) {
        if (button->text() == text && button->isVisible()) {
            return button;
        }
    }
    return nullptr;
}

void openBandFlyout(SpectrumOverlayPanel& overlay)
{
    for (QPushButton* button : overlay.findChildren<QPushButton*>()) {
        if (button->text() == QStringLiteral("BAND")) {
            button->click();
            return;
        }
    }
}

// A press Qt hands the popup itself, as one outside it arrives while it has the
// mouse. QTest warns about a point outside the widget, so this is sent directly.
void pressOutside(QWidget& popup)
{
    const QPoint local(-20, -20);
    QMouseEvent press(QEvent::MouseButtonPress, QPointF(local), QPointF(popup.mapToGlobal(local)),
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&popup, &press);
}

struct RouterFixture {
    std::unique_ptr<MainWindow> window;
    RadioModel* model{nullptr};
    SliceModel* a{nullptr};
    SliceModel* b{nullptr};
    SpectrumOverlayPanel* overlay{nullptr};
    ContainerWidget* containerOnB{nullptr};
    VfoDisplayItem* vfo{nullptr};
    FakeBandStackAccess fake;

    bool build()
    {
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
        model->setActiveSlice(0);
        a = model->sliceById(kSliceA);
        b = model->sliceById(kSliceB);
        window->resize(1400, 860);
        window->show();
        settle();
        overlay = window->findChild<SpectrumOverlayPanel*>();

        ContainerManager* manager = model->containerManager();
        containerOnB = manager ? manager->createContainer(1, DockMode::OverlayDocked) : nullptr;
        if (!a || !b || !overlay || !containerOnB) {
            return false;
        }
        containerOnB->setRxSource(kContainerRxSourceB);
        auto* meter = new MeterWidget();
        vfo = new VfoDisplayItem();
        vfo->setLayout(VfoDisplayItem::Layout::Thetis);
        vfo->setRect(0, 0, 1, 1);
        meter->addItem(vfo);
        containerOnB->setContent(meter);
        // A new overlay container sits raised at 0,0 at 640 by 480, over the pan's
        // flyout. Only its signals and its meter are used here, so it goes under
        // the pan and a click on the flyout reaches the flyout.
        containerOnB->lower();

        window->setBandStackAccessForTest(&fake);
        reset();
        return true;
    }

    // What the cases change, back as build() leaves it.
    void reset()
    {
        if (auto* popup = window->findChild<BandStackPopup*>()) {
            popup->close();
        }
        // The BAND button toggles, so it closes a flyout a case left open.
        if (flyoutButton(*overlay, QStringLiteral("20"))) {
            openBandFlyout(*overlay);
        }
        qDeleteAll(window->findChildren<StatusToast*>());
        fake.setDocument(QJsonObject{});
        fake.setState(BandStackAccess::State::Ready);
        model->setActiveSlice(0);
        a->setFrequency(kOn20mHz);
        b->setFrequency(kOn40mHz);
        settle();
        fake.calls.clear();
    }

    ~RouterFixture()
    {
        if (window) {
            window->setBandStackAccessForTest(model->bandStack());
        }
    }
};

} // namespace

class TstBandStackRouter : public QObject {
    Q_OBJECT

private:
    bool buildFixture()
    {
        m_fixture.reset();
        m_fixture = std::make_unique<RouterFixture>();
        m_rebuild = false;
        return m_fixture->build();
    }

    std::unique_ptr<RouterFixture> m_fixture;
    bool m_rebuild{false};

private slots:
    void initTestCase() { QVERIFY(buildFixture()); }

    void init()
    {
        if (m_rebuild) {
            QVERIFY(buildFixture());
        } else {
            m_fixture->reset();
        }
    }

    // A failed case may have left anything behind, so it does not get to spoil
    // the cases after it.
    void cleanup()
    {
        if (QTest::currentTestFailed()) {
            m_rebuild = true;
        }
    }

    void cleanupTestCase() { m_fixture.reset(); }

    // ── pressBand's decision ─────────────────────────────────────────────────
    void pressDecides_data()
    {
        QTest::addColumn<double>("sliceHz");
        QTest::addColumn<int>("mods");
        QTest::addColumn<QString>("verb");
        QTest::addColumn<bool>("previous");

        const int ctrl = int(Qt::ControlModifier);
        const int shift = int(Qt::ShiftModifier);
        // The press below is always for 20m.
        QTest::newRow("same plain") << kOn20mHz << 0 << QStringLiteral("press") << false;
        QTest::newRow("same ctrl") << kOn20mHz << ctrl << QStringLiteral("add") << false;
        QTest::newRow("same shift") << kOn20mHz << shift << QStringLiteral("press") << true;
        QTest::newRow("same ctrl shift") << kOn20mHz << (ctrl | shift) << QStringLiteral("add")
                                         << false;
        QTest::newRow("other plain") << kOn40mHz << 0 << QStringLiteral("press") << false;
        QTest::newRow("other ctrl") << kOn40mHz << ctrl << QStringLiteral("press") << false;
        QTest::newRow("other shift") << kOn40mHz << shift << QStringLiteral("press") << false;
    }

    void pressDecides()
    {
        QFETCH(double, sliceHz);
        QFETCH(int, mods);
        QFETCH(QString, verb);
        QFETCH(bool, previous);
        RouterFixture& f = *m_fixture;
        f.b->setFrequency(sliceHz);

        f.window->pressBand(f.b, Band::Band20m, Qt::KeyboardModifiers(mods));

        QCOMPARE(f.fake.calls.size(), 1);
        const FakeBandStackAccess::Call& call = f.fake.calls.first();
        QCOMPARE(call.verb, verb);
        QCOMPARE(call.sliceId, kSliceB);
        if (verb == QStringLiteral("press")) {
            QVERIFY(call.band == Band::Band20m);
            QCOMPARE(call.direction, previous ? BandStackDirection::Previous
                                              : BandStackDirection::Next);
        }
        // Nothing went down the old path as well.
        QCOMPARE(f.b->frequency(), sliceHz);
    }

    void withoutAReadyStackThePressIsTheOldOne_data()
    {
        QTest::addColumn<int>("state");
        QTest::newRow("older Core") << int(BandStackAccess::State::NeedsNewerCore);
        QTest::newRow("not connected") << int(BandStackAccess::State::NotConnected);
    }

    void withoutAReadyStackThePressIsTheOldOne()
    {
        QFETCH(int, state);
        RouterFixture& f = *m_fixture;
        f.fake.setState(static_cast<BandStackAccess::State>(state), QStringLiteral("Not now."));

        f.window->pressBand(f.b, Band::Band20m, Qt::ControlModifier);

        QVERIFY(f.fake.calls.isEmpty());
        // The band's seed or memory, on the slice named, and no other.
        QCOMPARE(BandDefaults::bandGridBandFromFrequency(f.b->frequency()), Band::Band20m);
        QCOMPARE(f.a->frequency(), kOn20mHz);

        // A same-band press stays what it was: nothing happens.
        const double before = f.b->frequency();
        f.window->pressBand(f.b, Band::Band20m, Qt::ShiftModifier);
        QVERIFY(f.fake.calls.isEmpty());
        QCOMPARE(f.b->frequency(), before);
    }

    void noSliceDoesNothing()
    {
        RouterFixture& f = *m_fixture;
        f.window->pressBand(nullptr, Band::Band20m, Qt::NoModifier);
        QVERIFY(f.fake.calls.isEmpty());
    }

    void theViewIsTheSlicePan()
    {
        RouterFixture& f = *m_fixture;
        SpectrumWidget* spectrum = f.window->spectrumForSlice(f.b);
        QVERIFY(spectrum);
        spectrum->setFrequencyRange(7'100'000.0, 96'000.0);
        const auto center = qRound64(spectrum->centerFrequency());
        const auto span = qRound64(spectrum->bandwidth());
        QVERIFY(span > 0);

        const BandStackView view = f.window->viewForSlice(f.b);
        QCOMPARE(view.centerHz.value_or(-1), center);
        QCOMPARE(view.spanHz.value_or(-1), span);
        QVERIFY(!f.window->viewForSlice(nullptr).centerHz.has_value());

        f.window->pressBand(f.b, Band::Band40m, Qt::NoModifier);
        QCOMPARE(f.fake.calls.size(), 1);
        QCOMPARE(f.fake.calls.first().view.centerHz.value_or(-1), center);
        QCOMPARE(f.fake.calls.first().view.spanHz.value_or(-1), span);
    }

    // ── Every path, with the modifiers held ──────────────────────────────────
    void everyPathGoesThroughTheRouter_data()
    {
        QTest::addColumn<QString>("path");
        QTest::addColumn<int>("mods");
        QTest::addColumn<QString>("verb");
        QTest::addColumn<bool>("previous");

        const int ctrl = int(Qt::ControlModifier);
        const int shift = int(Qt::ShiftModifier);
        for (const char* path : {"menu HF", "menu GEN", "menu WWV", "grid", "vfo", "flyout"}) {
            const QString name = QString::fromLatin1(path);
            QTest::addRow("%s plain", path) << name << 0 << QStringLiteral("press") << false;
            QTest::addRow("%s shift", path) << name << shift << QStringLiteral("press") << true;
            QTest::addRow("%s ctrl", path) << name << ctrl << QStringLiteral("add") << false;
        }
    }

    void everyPathGoesThroughTheRouter()
    {
        QFETCH(QString, path);
        QFETCH(int, mods);
        QFETCH(QString, verb);
        QFETCH(bool, previous);
        RouterFixture& f = *m_fixture;

        std::function<void()> press;
        int sliceId = kSliceA;
        Band band = Band::Band20m;
        if (path == QStringLiteral("menu HF")) {
            QAction* entry = bandMenuEntry(*f.window, QStringLiteral("20m"));
            QVERIFY(entry);
            press = [entry] { entry->trigger(); };
        } else if (path == QStringLiteral("menu GEN")) {
            QAction* entry = bandMenuEntry(*f.window, QStringLiteral("&GEN"));
            QVERIFY(entry);
            f.a->setFrequency(kOnGenHz);
            band = Band::GEN;
            press = [entry] { entry->trigger(); };
        } else if (path == QStringLiteral("menu WWV")) {
            QAction* entry = bandMenuEntry(*f.window, QStringLiteral("&WWV"));
            QVERIFY(entry);
            f.a->setFrequency(kOnWwvHz);
            band = Band::WWV;
            press = [entry] { entry->trigger(); };
        } else if (path == QStringLiteral("grid")) {
            // The container's own slice, which is not the active one.
            sliceId = kSliceB;
            band = Band::Band40m;
            ContainerWidget* container = f.containerOnB;
            press = [container, band] { emit container->bandClicked(static_cast<int>(band)); };
        } else if (path == QStringLiteral("vfo")) {
            sliceId = kSliceB;
            band = Band::Band40m;
            VfoDisplayItem* vfo = f.vfo;
            QVERIFY(vfo->property("vfoRequestsWired").toBool());
            press = [vfo, sliceId, band] { emit vfo->bandRequested(sliceId, band); };
        } else {
            SpectrumOverlayPanel* overlay = f.overlay;
            press = [overlay] {
                emit overlay->bandSelected(QStringLiteral("20m"), 14.0e6, QStringLiteral("USB"));
            };
        }
        QCOMPARE(BandDefaults::bandGridBandFromFrequency(
                     f.model->sliceById(sliceId)->frequency()), band);

        QWindow* handle = f.window->windowHandle();
        QVERIFY(handle);
        holdModifiers(handle, Qt::KeyboardModifiers(mods));
        const auto release = qScopeGuard([&] {
            releaseModifiers(handle, Qt::KeyboardModifiers(mods));
        });
        QCOMPARE(QGuiApplication::keyboardModifiers(), Qt::KeyboardModifiers(mods));

        press();

        QCOMPARE(f.fake.calls.size(), 1);
        const FakeBandStackAccess::Call& call = f.fake.calls.first();
        QCOMPARE(call.verb, verb);
        QCOMPARE(call.sliceId, sliceId);
        if (verb == QStringLiteral("press")) {
            QVERIFY(call.band == band);
            QCOMPARE(call.direction, previous ? BandStackDirection::Previous
                                              : BandStackDirection::Next);
        }
    }

    // A real flyout button in a real window, with Shift on the click itself.
    void aRealFlyoutClickCarriesItsModifier()
    {
        RouterFixture& f = *m_fixture;
        QPushButton* bandButton = nullptr;
        for (QPushButton* button : f.overlay->findChildren<QPushButton*>()) {
            if (button->text() == QStringLiteral("BAND")) {
                bandButton = button;
            }
        }
        QVERIFY(bandButton);
        bandButton->click();

        QPushButton* twenty = nullptr;
        for (QPushButton* button : f.overlay->parentWidget()->findChildren<QPushButton*>()) {
            if (button->text() == QStringLiteral("20") && button->isVisible()) {
                twenty = button;
            }
        }
        QVERIFY(twenty);
        QWidget* flyout = twenty->parentWidget();
        // The pan is a native surface, so the click goes in at the surface that
        // holds the flyout, not at the top-level window.
        QWidget* surface = twenty->nativeParentWidget();
        QVERIFY(surface && surface->windowHandle());
        const QPoint point = twenty->mapTo(surface, twenty->rect().center());
        // Nothing of the window's own covers the button. The pan is native on the
        // Mac and Windows but not on Linux, where the surface is the top-level
        // window, so a widget over the flyout takes the click there and only there.
        QCOMPARE(f.window->childAt(twenty->mapTo(f.window.get(), twenty->rect().center())),
                 static_cast<QWidget*>(twenty));

        QTest::mouseClick(surface->windowHandle(), Qt::LeftButton, Qt::ShiftModifier, point);

        QCOMPARE(f.fake.calls.size(), 1);
        QCOMPARE(f.fake.calls.first().verb, QStringLiteral("press"));
        QCOMPARE(f.fake.calls.first().sliceId, kSliceA);
        QCOMPARE(f.fake.calls.first().direction, BandStackDirection::Previous);
        QVERIFY(flyout->isVisible());

        QTest::mouseClick(surface->windowHandle(), Qt::LeftButton, Qt::NoModifier, point);
        QCOMPARE(f.fake.calls.size(), 2);
        QCOMPARE(f.fake.calls.last().direction, BandStackDirection::Next);
        QVERIFY(flyout->isVisible());
    }

    // ── STACK+ and STACK- ────────────────────────────────────────────────────
    void stackButtonsStepTheContainerSlice_data()
    {
        QTest::addColumn<int>("id");
        QTest::addColumn<bool>("previous");
        QTest::newRow("STACK+") << int(OtherButtonItem::ButtonId::StackUp) << false;
        QTest::newRow("STACK-") << int(OtherButtonItem::ButtonId::StackDown) << true;
    }

    void stackButtonsStepTheContainerSlice()
    {
        QFETCH(int, id);
        QFETCH(bool, previous);
        RouterFixture& f = *m_fixture;

        emit f.containerOnB->otherButtonClicked(id);

        QCOMPARE(f.fake.calls.size(), 1);
        QCOMPARE(f.fake.calls.first().verb, QStringLiteral("step"));
        QCOMPARE(f.fake.calls.first().sliceId, kSliceB);
        QCOMPARE(f.fake.calls.first().direction, previous ? BandStackDirection::Previous
                                                          : BandStackDirection::Next);
    }

    void stackButtonsAreOfferedAndGreyWithTheReason()
    {
        RouterFixture& f = *m_fixture;
        auto* buttons = new OtherButtonItem();
        auto* meter = qobject_cast<MeterWidget*>(f.containerOnB->content());
        meter->addItem(buttons);
        const auto removeButtons = qScopeGuard([meter, buttons] { meter->removeItem(buttons); });
        settle();
        QVERIFY(buttons->isOffered(OtherButtonItem::ButtonId::StackUp));
        QVERIFY(buttons->isOffered(OtherButtonItem::ButtonId::StackDown));
        QVERIFY(buttons->isButtonAvailable(OtherButtonItem::ButtonId::StackUp));
        QVERIFY(buttons->isButtonAvailable(OtherButtonItem::ButtonId::StackDown));

        const QString reason = QStringLiteral("This Core does not have band stacks.");
        f.fake.setState(BandStackAccess::State::NeedsNewerCore, reason);
        settle();
        QVERIFY(!buttons->isButtonAvailable(OtherButtonItem::ButtonId::StackUp));
        QVERIFY(!buttons->isButtonAvailable(OtherButtonItem::ButtonId::StackDown));

        emit f.containerOnB->otherButtonClicked(int(OtherButtonItem::ButtonId::StackUp));
        QVERIFY(f.fake.calls.isEmpty());
        QCOMPARE(toastCount(*f.window, reason), 1);

        f.fake.setState(BandStackAccess::State::Ready);
        settle();
        QVERIFY(buttons->isButtonAvailable(OtherButtonItem::ButtonId::StackUp));
    }

    // ── The entry points, found by name as the hot keys find them ────────────
    void entryPointsActOnTheSelectedSlice()
    {
        RouterFixture& f = *m_fixture;
        f.model->setActiveSlice(1);

        QVERIFY(QMetaObject::invokeMethod(f.window.get(), "bandStackNext"));
        QVERIFY(QMetaObject::invokeMethod(f.window.get(), "bandStackPrevious"));
        QVERIFY(QMetaObject::invokeMethod(f.window.get(), "bandStackAdd"));
        QCOMPARE(f.fake.calls.size(), 3);
        QCOMPARE(f.fake.calls.at(0).verb, QStringLiteral("step"));
        QCOMPARE(f.fake.calls.at(0).direction, BandStackDirection::Next);
        QCOMPARE(f.fake.calls.at(1).verb, QStringLiteral("step"));
        QCOMPARE(f.fake.calls.at(1).direction, BandStackDirection::Previous);
        QCOMPARE(f.fake.calls.at(2).verb, QStringLiteral("add"));
        for (const FakeBandStackAccess::Call& call : f.fake.calls) {
            QCOMPARE(call.sliceId, kSliceB);
            QVERIFY(call.view.centerHz.has_value());
        }
        QVERIFY(QMetaObject::invokeMethod(f.window.get(), "bandStackPanel"));
    }

    void entryPointsSayWhyWhenNotReady_data()
    {
        QTest::addColumn<QString>("slot");
        QTest::newRow("next") << QStringLiteral("bandStackNext");
        QTest::newRow("previous") << QStringLiteral("bandStackPrevious");
        QTest::newRow("add") << QStringLiteral("bandStackAdd");
    }

    void entryPointsSayWhyWhenNotReady()
    {
        QFETCH(QString, slot);
        RouterFixture& f = *m_fixture;
        const QString reason = QStringLiteral("Connect to a radio to use band stacks.");
        f.fake.setState(BandStackAccess::State::NotConnected, reason);
        QCOMPARE(toastCount(*f.window, reason), 0);

        QVERIFY(QMetaObject::invokeMethod(f.window.get(), slot.toLatin1().constData()));

        QCOMPARE(toastCount(*f.window, reason), 1);
        QVERIFY(f.fake.calls.isEmpty());
    }

    // ── The popup ────────────────────────────────────────────────────────────
    void popupOpensForEachPath_data()
    {
        QTest::addColumn<QString>("path");
        QTest::addColumn<int>("sliceId");
        QTest::addColumn<int>("band");
        QTest::addColumn<QStringList>("rows");

        QTest::newRow("flyout 20m") << QStringLiteral("20") << kSliceA << int(Band::Band20m)
                                    << QStringList{QStringLiteral("ft8")};
        QTest::newRow("flyout WWV") << QStringLiteral("WWV") << kSliceA << int(Band::WWV)
                                    << QStringList{QStringLiteral("time")};
        // Band number 0, which the VFO display's own request also carries.
        QTest::newRow("container 160m") << QStringLiteral("container") << kSliceB
                                        << int(Band::Band160m)
                                        << QStringList{QStringLiteral("top")};
        QTest::newRow("container WWV") << QStringLiteral("container") << kSliceB << int(Band::WWV)
                                       << QStringList{QStringLiteral("time")};
        // Slice B is on 40m.
        QTest::newRow("vfo") << QStringLiteral("vfo") << kSliceB << int(Band::Band40m)
                             << QStringList{QStringLiteral("net"), QStringLiteral("ssb"),
                                            QStringLiteral("dx")};
    }

    void popupOpensForEachPath()
    {
        QFETCH(QString, path);
        QFETCH(int, sliceId);
        QFETCH(int, band);
        QFETCH(QStringList, rows);
        RouterFixture& f = *m_fixture;
        f.fake.setDocument(popupDocument());

        if (path == QStringLiteral("container")) {
            emit f.containerOnB->bandStackRequested(band);
        } else if (path == QStringLiteral("vfo")) {
            emit f.vfo->bandStackRequested(0);
        } else {
            openBandFlyout(*f.overlay);
            QPushButton* button = flyoutButton(*f.overlay, path);
            QVERIFY(button);
            emit button->customContextMenuRequested(QPoint(3, 3));
        }

        auto* popup = f.window->findChild<BandStackPopup*>();
        QVERIFY(popup);
        const auto close = qScopeGuard([popup] { popup->close(); });
        QVERIFY(popup->isVisible());
        QVERIFY(popup->band() == static_cast<Band>(band));
        QCOMPARE(popup->rows()->sliceId(), sliceId);
        QCOMPARE(popup->rows()->visibleRowIds(), rows);
        QVERIFY(popup->shownReason().isEmpty());
        // Opening it sends nothing to the Core.
        QVERIFY(f.fake.calls.isEmpty());
    }

    void popupOpensAtThePointerAndStaysOnScreen()
    {
        RouterFixture& f = *m_fixture;
        f.fake.setDocument(popupDocument(14));

        f.window->openBandStackPopup(f.b, Band::Band20m, QPoint(100, 100));
        auto* popup = f.window->findChild<BandStackPopup*>();
        QVERIFY(popup);
        const auto close = qScopeGuard([popup] { popup->close(); });
        // Beside the pointer. The offscreen platform adds a 2 px window frame that
        // a real popup does not have.
        QVERIFY2(popup->x() >= 102 && popup->x() <= 104 && popup->y() >= 102 && popup->y() <= 104,
                 qPrintable(QStringLiteral("%1,%2").arg(popup->x()).arg(popup->y())));
        QCOMPARE(popup->width(), BandStackPopup::kWidth);

        // A long band stops at the mockup's height and scrolls.
        popup->close();
        f.window->openBandStackPopup(f.b, Band::Band40m, QPoint(100, 100));
        QCOMPARE(popup->height(), BandStackPopup::kMaxHeight);
        QCOMPARE(popup->rows()->visibleRowIds().size(), 17);

        const QRect screen = popup->screen()->availableGeometry();
        popup->close();
        f.window->openBandStackPopup(f.b, Band::Band40m, screen.bottomRight());
        QVERIFY2(screen.contains(popup->geometry()),
                 qPrintable(QStringLiteral("%1,%2 %3x%4")
                                .arg(popup->x()).arg(popup->y())
                                .arg(popup->width()).arg(popup->height())));
    }

    void popupRowsActAndKeepItOpen()
    {
        RouterFixture& f = *m_fixture;
        f.fake.setDocument(popupDocument());
        f.window->openBandStackPopup(f.b, Band::Band40m, QPoint(100, 100));
        auto* popup = f.window->findChild<BandStackPopup*>();
        QVERIFY(popup);
        const auto close = qScopeGuard([popup] { popup->close(); });
        QTRY_VERIFY(popup->rows()->rowRect(QStringLiteral("dx")).isValid());
        BandStackRowList* rows = popup->rows();

        QTest::mouseClick(rows, Qt::LeftButton, Qt::NoModifier,
                          rows->rowRect(QStringLiteral("dx")).center());
        QCOMPARE(f.fake.calls.size(), 1);
        QCOMPARE(f.fake.calls.first().verb, QStringLiteral("goTo"));
        QCOMPARE(f.fake.calls.first().sliceId, kSliceB);
        QCOMPARE(f.fake.calls.first().entryId, QStringLiteral("dx"));
        QVERIFY(popup->isVisible());

        // Slice B is on this band, so the add row is there.
        QVERIFY(rows->addRect().isValid());
        QTest::mouseClick(rows, Qt::LeftButton, Qt::NoModifier, rows->addRect().center());
        QCOMPARE(f.fake.calls.size(), 2);
        QCOMPARE(f.fake.calls.last().verb, QStringLiteral("add"));
        QCOMPARE(f.fake.calls.last().sliceId, kSliceB);
        QVERIFY(popup->isVisible());

        // No drag in a popup.
        const QPoint from = rows->rowRect(QStringLiteral("net")).center();
        const QPoint to = rows->rowRect(QStringLiteral("dx")).center();
        QTest::mousePress(rows, Qt::LeftButton, Qt::NoModifier, from);
        QMouseEvent move(QEvent::MouseMove, QPointF(to), QPointF(rows->mapToGlobal(to)),
                         Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(rows, &move);
        QVERIFY(!rows->isDragging());
        QTest::mouseRelease(rows, Qt::LeftButton, Qt::NoModifier, to);
        for (const FakeBandStackAccess::Call& call : std::as_const(f.fake.calls)) {
            QVERIFY(call.verb != QStringLiteral("reorder"));
        }

        // The slice leaves the band: the add row goes, and the popup fits again.
        const int heightBefore = popup->height();
        f.b->setFrequency(kOn20mHz);
        QVERIFY(!rows->addRect().isValid());
        QVERIFY(popup->height() < heightBefore);
    }

    void popupRowMenuDoesNotCloseIt()
    {
        RouterFixture& f = *m_fixture;
        f.fake.setDocument(popupDocument());
        f.window->openBandStackPopup(f.b, Band::Band40m, QPoint(100, 100));
        auto* popup = f.window->findChild<BandStackPopup*>();
        QVERIFY(popup);
        const auto close = qScopeGuard([popup] { popup->close(); });
        BandStackRowList* rows = popup->rows();
        const QPoint at = rows->rowRect(QStringLiteral("dx")).center();

        // The row menu runs its own loop, so a timer works it.
        QStringList seen;
        QTimer poll;
        poll.setInterval(5);
        connect(&poll, &QTimer::timeout, &poll, [&] {
            auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
            if (!menu) {
                return;
            }
            poll.stop();
            for (QAction* action : menu->actions()) {
                // Without the shortcut that follows a tab.
                seen << action->text().section(u'\t', 0, 0);
                if (seen.last() == QStringLiteral("Lock")) {
                    action->trigger();
                }
            }
            menu->close();
        });
        poll.start();
        QContextMenuEvent event(QContextMenuEvent::Mouse, at, rows->mapToGlobal(at));
        QApplication::sendEvent(rows, &event);

        QVERIFY2(seen.contains(QStringLiteral("Go there")), qPrintable(seen.join(u',')));
        QVERIFY2(seen.contains(QStringLiteral("Delete")), qPrintable(seen.join(u',')));
        QCOMPARE(f.fake.calls.size(), 1);
        QCOMPARE(f.fake.calls.first().verb, QStringLiteral("edit"));
        QCOMPARE(f.fake.calls.first().entryId, QStringLiteral("dx"));
        QVERIFY(popup->isVisible());
        QCOMPARE(QApplication::activePopupWidget(), popup);
    }

    void popupClosesOnAnOutsideClickAndEsc()
    {
        RouterFixture& f = *m_fixture;
        f.fake.setDocument(popupDocument());
        f.window->openBandStackPopup(f.b, Band::Band40m, QPoint(100, 100));
        auto* popup = f.window->findChild<BandStackPopup*>();
        QVERIFY(popup);
        QVERIFY(popup->isVisible());
        pressOutside(*popup);
        QVERIFY(!popup->isVisible());

        f.window->openBandStackPopup(f.b, Band::Band40m, QPoint(100, 100));
        QVERIFY(popup->isVisible());
        QTest::keyClick(popup->rows(), Qt::Key_Escape);
        QVERIFY(!popup->isVisible());

        // A slice that goes away takes its popup with it. Everything else in the
        // window hears this too, though the slice is still there, so the next
        // case gets a new window.
        f.window->openBandStackPopup(f.b, Band::Band40m, QPoint(100, 100));
        QVERIFY(popup->isVisible());
        m_rebuild = true;
        emit f.model->sliceRemoved(kSliceB);
        QVERIFY(!popup->isVisible());
    }

    void popupLeavesTheFlyoutOpenUntilAClickOutsideBoth()
    {
        RouterFixture& f = *m_fixture;
        f.fake.setDocument(popupDocument());
        openBandFlyout(*f.overlay);
        QPushButton* twenty = flyoutButton(*f.overlay, QStringLiteral("20"));
        QVERIFY(twenty);
        QWidget* flyout = twenty->parentWidget();
        emit twenty->customContextMenuRequested(QPoint(3, 3));
        auto* popup = f.window->findChild<BandStackPopup*>();
        QVERIFY(popup);
        const auto close = qScopeGuard([popup] { popup->close(); });
        QVERIFY(popup->isVisible());
        QVERIFY(flyout->isVisible());

        BandStackRowList* rows = popup->rows();
        QTest::mouseClick(rows, Qt::LeftButton, Qt::NoModifier,
                          rows->rowRect(QStringLiteral("ft8")).center());
        QCOMPARE(f.fake.calls.size(), 1);
        QCOMPARE(f.fake.calls.first().verb, QStringLiteral("goTo"));
        QVERIFY(popup->isVisible());
        QVERIFY(flyout->isVisible());

        // Outside both: the popup goes on the press itself, and the press Qt
        // replays behind it finds no popup up and closes the flyout.
        pressOutside(*popup);
        QVERIFY(!popup->isVisible());
        QVERIFY(flyout->isVisible());
        QWidget* surface = twenty->nativeParentWidget();
        QVERIFY(surface && surface->windowHandle());
        const QPoint away(surface->width() - 20, surface->height() - 20);
        QTest::mouseClick(surface->windowHandle(), Qt::LeftButton, Qt::NoModifier, away);
        QVERIFY(!flyout->isVisible());
    }

    void popupShowsTheReasonInsteadOfRows_data()
    {
        QTest::addColumn<int>("state");
        QTest::addColumn<QString>("reason");
        QTest::newRow("older Core")
            << int(BandStackAccess::State::NeedsNewerCore)
            << QStringLiteral("This Core does not have band stacks. Updating the Core may help.");
        QTest::newRow("not connected") << int(BandStackAccess::State::NotConnected)
                                       << BandStackAccess::notConnectedReason();
    }

    void popupShowsTheReasonInsteadOfRows()
    {
        QFETCH(int, state);
        QFETCH(QString, reason);
        RouterFixture& f = *m_fixture;
        f.fake.setDocument(popupDocument());
        f.fake.setState(static_cast<BandStackAccess::State>(state), reason);

        f.window->openBandStackPopup(f.b, Band::Band40m, QPoint(100, 100));
        auto* popup = f.window->findChild<BandStackPopup*>();
        QVERIFY(popup);
        const auto close = qScopeGuard([popup] { popup->close(); });
        QVERIFY(popup->isVisible());
        QCOMPARE(popup->shownReason(), reason);
        auto* label = popup->findChild<QLabel*>(QStringLiteral("bandStackPopupReason"));
        auto* list = popup->findChild<QScrollArea*>(QStringLiteral("bandStackPopupRows"));
        QVERIFY(label && list);
        QVERIFY(label->isVisible());
        QCOMPARE(label->text(), reason);
        QVERIFY(!list->isVisible());
        QVERIFY(f.fake.calls.isEmpty());

        // The stack comes back while it is open.
        f.fake.setState(BandStackAccess::State::Ready);
        QVERIFY(popup->shownReason().isEmpty());
        QVERIFY(!label->isVisible());
        QVERIFY(list->isVisible());
    }

    void popupWithNoSliceSaysSo()
    {
        RouterFixture& f = *m_fixture;
        f.fake.setDocument(popupDocument());
        f.window->openBandStackPopup(nullptr, Band::Band40m, QPoint(100, 100));
        auto* popup = f.window->findChild<BandStackPopup*>();
        QVERIFY(popup);
        const auto close = qScopeGuard([popup] { popup->close(); });
        QVERIFY(popup->isVisible());
        QCOMPARE(popup->shownReason(), QStringLiteral("Select a slice first."));
    }

    void popupCaptures_data()
    {
        QTest::addColumn<bool>("workspaceLook");
        QTest::newRow("workspace look") << true;
        QTest::newRow("classic look") << false;
    }

    void popupCaptures()
    {
        QFETCH(bool, workspaceLook);
        const QString directory = qEnvironmentVariable("NEREUS_BANDSTACK_CAPTURE_DIR");
        if (directory.isEmpty()) {
            return;
        }
        WorkspacePreview::setEnabledForTest(workspaceLook);
        const auto restore = qScopeGuard([] { WorkspacePreview::setEnabledForTest(false); });
        auto& app = *qobject_cast<QApplication*>(QCoreApplication::instance());
        applyDarkPalette(app);
        applyAppBaselineQss(app);
        if (workspaceLook) {
            applyWorkspacePalette(app);
        }
        const QString look = workspaceLook ? QStringLiteral("workspace") : QStringLiteral("classic");
        const auto save = [&](QWidget& popup, const char* name) {
            settle();
            QVERIFY(popup.grab().save(QDir(directory).filePath(
                QStringLiteral("%1-popup-%2.png").arg(look, QString::fromLatin1(name)))));
        };
        // A window of its own, built in the look it is captured in.
        QVERIFY(buildFixture());
        m_rebuild = true;
        RouterFixture& f = *m_fixture;
        f.fake.setDocument(popupDocument());
        f.window->openBandStackPopup(f.b, Band::Band40m, QPoint(100, 100));
        auto* popup = f.window->findChild<BandStackPopup*>();
        QVERIFY(popup);
        const auto close = qScopeGuard([popup] { popup->close(); });
        save(*popup, "on-band");
        popup->close();
        f.window->openBandStackPopup(f.a, Band::Band40m, QPoint(100, 100));
        save(*popup, "other-band");
        popup->close();
        f.fake.setDocument(popupDocument(14));
        f.window->openBandStackPopup(f.b, Band::Band40m, QPoint(100, 100));
        save(*popup, "long");
        popup->close();
        f.fake.setState(BandStackAccess::State::NeedsNewerCore,
                        QStringLiteral("This Core does not have band stacks. Updating the Core may help."));
        f.window->openBandStackPopup(f.b, Band::Band40m, QPoint(100, 100));
        save(*popup, "older-core");
        popup->close();
        f.fake.setState(BandStackAccess::State::NotConnected, BandStackAccess::notConnectedReason());
        f.window->openBandStackPopup(f.b, Band::Band40m, QPoint(100, 100));
        save(*popup, "not-connected");
    }

    // ── What comes back ──────────────────────────────────────────────────────
    void refusalsAndReportsToastOnce()
    {
        RouterFixture& f = *m_fixture;
        const QString refusal = QStringLiteral("Band 20m ignored: the slice is locked.");
        const QString report = QStringLiteral("20m: 14.074 USB, 1 of 2");
        emit f.fake.refused(refusal);
        emit f.fake.reported(report);
        QCOMPARE(toastCount(*f.window, refusal), 1);
        QCOMPARE(toastCount(*f.window, report), 1);
    }
};

QTEST_MAIN(TstBandStackRouter)
#include "tst_bandstack_router.moc"
