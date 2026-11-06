// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_bandstack_router.cpp  (NereusSDR)
// =================================================================
//
// Every band press in the desktop goes through MainWindow::pressBand. A real
// MainWindow offscreen with a fake BandStackAccess: which verb, which slice and
// which view each path sends, with the modifiers held, and the old path when the
// stack is not ready. Also STACK+ and STACK-, the four hot key entry points, and
// one toast per refusal or report.
//
// Design: docs/architecture/2026-11-05-band-stack-design.md, sections 5.1 and 7.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-06 - New for NereusSDR by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <QAction>
#include <QApplication>
#include <QGridLayout>
#include <QGuiApplication>
#include <QMenu>
#include <QPushButton>
#include <QScopeGuard>
#include <QWindow>

#include <functional>
#include <memory>

#include "core/RadioDiscovery.h"
#include "fakes/FakeBandStackAccess.h"
#include "gui/MainWindow.h"
#include "gui/SpectrumOverlayPanel.h"
#include "gui/SpectrumWidget.h"
#include "gui/containers/ContainerManager.h"
#include "gui/containers/ContainerWidget.h"
#include "gui/meters/MeterWidget.h"
#include "gui/meters/OtherButtonItem.h"
#include "gui/meters/VfoDisplayItem.h"
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
        containerOnB->setContent(meter);
        vfo = new VfoDisplayItem();
        vfo->setLayout(VfoDisplayItem::Layout::Thetis);
        vfo->setRect(0, 0, 1, 1);
        meter->addItem(vfo);
        a->setFrequency(kOn20mHz);
        b->setFrequency(kOn40mHz);

        window->setBandStackAccessForTest(&fake);
        fake.setState(BandStackAccess::State::Ready);
        fake.calls.clear();
        return true;
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

private slots:
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
        RouterFixture f;
        QVERIFY(f.build());
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
        RouterFixture f;
        QVERIFY(f.build());
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
        RouterFixture f;
        QVERIFY(f.build());
        f.window->pressBand(nullptr, Band::Band20m, Qt::NoModifier);
        QVERIFY(f.fake.calls.isEmpty());
    }

    void theViewIsTheSlicePan()
    {
        RouterFixture f;
        QVERIFY(f.build());
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
        RouterFixture f;
        QVERIFY(f.build());

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
        RouterFixture f;
        QVERIFY(f.build());
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
        RouterFixture f;
        QVERIFY(f.build());

        emit f.containerOnB->otherButtonClicked(id);

        QCOMPARE(f.fake.calls.size(), 1);
        QCOMPARE(f.fake.calls.first().verb, QStringLiteral("step"));
        QCOMPARE(f.fake.calls.first().sliceId, kSliceB);
        QCOMPARE(f.fake.calls.first().direction, previous ? BandStackDirection::Previous
                                                          : BandStackDirection::Next);
    }

    void stackButtonsAreOfferedAndGreyWithTheReason()
    {
        RouterFixture f;
        QVERIFY(f.build());
        auto* buttons = new OtherButtonItem();
        qobject_cast<MeterWidget*>(f.containerOnB->content())->addItem(buttons);
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
        RouterFixture f;
        QVERIFY(f.build());
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
        RouterFixture f;
        QVERIFY(f.build());
        const QString reason = QStringLiteral("Connect to a radio to use band stacks.");
        f.fake.setState(BandStackAccess::State::NotConnected, reason);
        QCOMPARE(toastCount(*f.window, reason), 0);

        QVERIFY(QMetaObject::invokeMethod(f.window.get(), slot.toLatin1().constData()));

        QCOMPARE(toastCount(*f.window, reason), 1);
        QVERIFY(f.fake.calls.isEmpty());
    }

    // ── What comes back ──────────────────────────────────────────────────────
    void refusalsAndReportsToastOnce()
    {
        RouterFixture f;
        QVERIFY(f.build());
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
