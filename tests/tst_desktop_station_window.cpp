// no-port-check: NereusSDR-original. Desktop host presentation over a borrowed local model.
// Modification history (NereusSDR):
//   2026-10-02  J.J. Boyd / KG4VCF. Real window/fake Core TX-letter Take
//                regressions: cancellation, current refusal, unchanged RX
//                history, and target lifetime. AI-assisted via OpenAI Codex.

#include "gui/HostingSliceActions.h"
#include "gui/MainWindow.h"

#include "core/FFTRouter.h"
#include "core/TxSliceArbiter.h"
#include "core/AppSettings.h"
#include "core/MoxController.h"
#include "core/SliceOwnership.h"
#include "core/TciServer.h"
#include "core/TwoToneController.h"
#include "core/TxChannel.h"
#include "core/safety/TransmitHolder.h"
#include "core/safety/TxRefusal.h"
#include "core/session/DeviceSessionRegistry.h"
#include "core/session/StationServer.h"
#include "gui/containers/ContainerManager.h"
#include "gui/containers/ContainerWidget.h"
#include "gui/meters/MeterWidget.h"
#include "gui/meters/OtherButtonItem.h"
#include "gui/applets/RxApplet.h"
#include "gui/applets/TxApplet.h"
#include "gui/multidevice/NoticeCard.h"
#include "gui/multidevice/TakeTransmitDialog.h"
#include "core/session/SliceAccessController.h"
#include "gui/SpectrumWidget.h"
#include "gui/PanadapterApplet.h"
#include "gui/PanadapterStack.h"
#include "gui/widgets/VfoWidget.h"
#include "gui/widgets/RxDashboard.h"
#include "gui/widgets/StatusToast.h"
#include "gui/SliceChooser.h"
#include "gui/PanFloatingWindow.h"
#include "gui/SetupDialog.h"
#include "gui/setup/DspSetupPages.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"
#include "models/NotchModel.h"

#include <QImage>
#include <QLabel>
#include <QMenu>
#include <QPointer>
#include <QPainter>
#include <QPushButton>
#include <QSignalSpy>
#include <QSpinBox>
#include <QToolButton>
#include <QWheelEvent>
#include <QApplication>
#include <QSslSocket>
#include <QTcpServer>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

using namespace NereusSDR;

namespace {

// Fix round 2 (minor 2): a TX channel that records the two-tone
// generator's run state, so a test sees the 2-tone test start.
class ToneRecordingTxChannel : public TxChannel {
public:
    ToneRecordingTxChannel() : TxChannel(1) {}
    void setTxPostGenMode(int) override {}
    void setTxPostGenTTFreq1(double) override {}
    void setTxPostGenTTFreq2(double) override {}
    void setTxPostGenTTMag1(double) override {}
    void setTxPostGenTTMag2(double) override {}
    void setTxPostGenTTPulseToneFreq1(double) override {}
    void setTxPostGenTTPulseToneFreq2(double) override {}
    void setTxPostGenTTPulseMag1(double) override {}
    void setTxPostGenTTPulseMag2(double) override {}
    void setTxPostGenTTPulseFreq(int) override {}
    void setTxPostGenTTPulseDutyCycle(double) override {}
    void setTxPostGenTTPulseTransition(double) override {}
    void setTxPostGenTTPulseIQOut(bool) override {}
    void setTxPostGenRun(bool on) override { runs.append(on); }
    QList<bool> runs;
};
StationHostOptions optionsFor(AppSettings& settings, const QString& directory)
{
    StationHostOptions options;
    options.settings = &settings;
    options.securityDirectory = directory;
    options.hostingDevice = StationHostOptions::HostingDevice{
        QStringLiteral("Shack Mac mini"), QStringLiteral("Shack")};
    options.remoteBind = QStringLiteral("127.0.0.1");
    options.statusPage = false;
    QTcpServer probe;
    if (probe.listen(QHostAddress::LocalHost, 0)) {
        options.remotePort = probe.serverPort();
        probe.close();
    }
    return options;
}

VfoWidget* flagFor(MainWindow& window, int id)
{
    for (VfoWidget* flag : window.findChildren<VfoWidget*>()) {
        if (flag->sliceIndex() == id) { return flag; }
    }
    return nullptr;
}

QPushButton* txLetterFor(MainWindow& window, int id)
{
    TxApplet* applet = window.findChild<TxApplet*>();
    if (!applet) { return nullptr; }
    for (QPushButton* button : applet->transmitSliceButtons()) {
        if (button->property("sliceId").toInt() == id) { return button; }
    }
    return nullptr;
}

QMenu* menuNamed(MainWindow& window, const QString& name)
{
    for (QMenu* menu : window.findChildren<QMenu*>()) {
        if (menu->title().remove(QLatin1Char('&')) == name) { return menu; }
    }
    return nullptr;
}

QAction* actionNamed(QMenu* menu, const QString& name)
{
    if (!menu) { return nullptr; }
    for (QAction* action : menu->actions()) {
        if (action->text().remove(QLatin1Char('&')) == name) { return action; }
    }
    return nullptr;
}

QToolButton* sliceTabFor(RxApplet& applet, QChar letter)
{
    for (QToolButton* tab : applet.findChildren<QToolButton*>()) {
        if (tab->isCheckable() && tab->text() == QString(letter)
            && tab->toolTip().startsWith(QStringLiteral("Slice %1").arg(letter))) {
            return tab;
        }
    }
    return nullptr;
}

DeviceSessionRegistry::Entry admitPhone(StationServer& server, QObject& session,
                                        const QByteArray& deviceId)
{
    DeviceSessionRegistry::Entry phone;
    phone.deviceId = deviceId;
    phone.kind = DeviceSessionRegistry::Kind::Paired;
    phone.name = QStringLiteral("Living room iPhone");
    phone.shortName = QStringLiteral("iPhone");
    phone.deviceKind = QStringLiteral("phone");
    const bool admitted = server.deviceSessions()->admit(phone, &session).admission
        == DeviceSessionRegistry::Admission::Admitted;
    if (!admitted) { phone.deviceId.clear(); }
    return phone;
}

int toastsSaying(MainWindow& window, const QString& words)
{
    int count = 0;
    for (StatusToast* toast : window.findChildren<StatusToast*>()) {
        if (toast->message() == words) { ++count; }
    }
    return count;
}

int flagCountFor(MainWindow& window, int id)
{
    int count = 0;
    for (VfoWidget* flag : window.findChildren<VfoWidget*>()) {
        if (flag->sliceIndex() == id) { ++count; }
    }
    return count;
}

bool applyLayout(MainWindow& window, const QString& layoutId)
{
    return QMetaObject::invokeMethod(&window, "applyPanLayout", Q_ARG(QString, layoutId));
}
}

// A layout change that retires the pan of a slice this window listens to:
// A and C (controlled here) share pan-0's stream, B (the phone's) sits
// 1 MHz above on its own stream and pan-1. Going to layout "1" places B on
// pan-0, the pan that remains.
struct PlacedListen
{
    QTemporaryDir directory;
    std::unique_ptr<AppSettings> settings;
    std::unique_ptr<MainWindow> window;
    std::unique_ptr<DesktopStationController> controller;
    QObject phoneSession;
    RadioModel* model{nullptr};
    PanadapterStack* stack{nullptr};
    SpectrumWidget* remaining{nullptr};
    QByteArray phone;
    int aId{-1};
    int bId{-1};
    int cId{-1};
    double viewCentre{0.0};
    double viewSpan{0.0};

    ~PlacedListen()
    {
        if (controller) { controller->stop(); }
    }
};

// Everything up to the layout change; placeListenedSlice() makes it.
void prepareListenedSlice(PlacedListen& f, bool ctun)
{
    QVERIFY(f.directory.isValid());
    f.settings = std::make_unique<AppSettings>(
        f.directory.filePath(QStringLiteral("station.settings")));
    f.window = std::make_unique<MainWindow>(RemoteStationOptions{}, nullptr,
                                            MainWindow::ConnectionStartup::Deferred);
    f.model = f.window->radioModel();
    f.model->setBoardForTest(HPSDRHW::Saturn);
    f.model->configureStreamPool(5, 5, 192000);
    f.model->setConnectionStateForTest(ConnectionState::Connected);
    f.aId = f.model->addSlice(QStringLiteral("pan-0"));
    f.bId = f.model->addSlice(QStringLiteral("pan-1"));
    QVERIFY(applyLayout(*f.window, QStringLiteral("2v")));
    f.stack = f.window->findChild<PanadapterStack*>();
    QVERIFY(f.stack);
    f.cId = f.model->addSlice(QStringLiteral("pan-1"));
    QVERIFY(f.cId >= 0);
    SliceModel* b = f.model->sliceById(f.bId);
    QVERIFY(b);
    b->setFrequency(f.model->sliceById(f.aId)->frequency() + 1'000'000.0);
    QVERIFY(b->streamIndex() != f.model->sliceById(f.aId)->streamIndex());
    f.controller = std::make_unique<DesktopStationController>(
        f.model, optionsFor(*f.settings, f.directory.path()));
    f.window->setDesktopStationController(f.controller.get());
    QVERIFY(f.controller->start(true));
    QVERIFY(f.controller->server());
    f.phone = admitPhone(*f.controller->server(), f.phoneSession,
                         QByteArrayLiteral("phone-device-id-for-layouts-0002")).deviceId;
    QVERIFY(!f.phone.isEmpty());
    f.model->sliceOwnership()->setOwner(f.bId, f.phone);
    QVERIFY(f.model->sliceOwnership()->isListening(SliceOwnership::stationDevice(), f.bId));
    f.remaining = f.stack->spectrum(QStringLiteral("pan-0"));
    QVERIFY(f.remaining);
    if (!ctun) {
        f.remaining->setCtunEnabled(false);
        QVERIFY(!f.remaining->ctunEnabled());
    }
    f.viewCentre = f.remaining->centerFrequency();
    f.viewSpan = f.remaining->bandwidth();
}

void placeListenedSlice(PlacedListen& f)
{
    QVERIFY(applyLayout(*f.window, QStringLiteral("1")));
    QCOMPARE(f.stack->currentLayoutId(), QStringLiteral("1"));
    QCOMPARE(f.stack->spectrum(QStringLiteral("pan-0")), f.remaining);
    QVERIFY(f.model->sliceOwnership()->isListening(SliceOwnership::stationDevice(), f.bId));
}

// The flag the pan that remains shows for `id`.
VfoWidget* flagOn(SpectrumWidget* pan, int id)
{
    VfoWidget* found = nullptr;
    for (VfoWidget* flag : pan->findChildren<VfoWidget*>()) {
        if (flag->sliceIndex() == id) { found = flag; }
    }
    return found;
}

// Pixels of `id`'s colour in the pan's edge-marker layer, on one side.
int edgeMarkerPixels(SpectrumWidget* pan, int id, bool rightSide)
{
    QImage image(800, 400, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::black);
    {
        QPainter painter(&image);
        pan->drawOffScreenIndicatorForTest(painter, QRect(0, 0, 800, 180),
                                           QRect(0, 200, 800, 180));
    }
    const QRgb colour = VfoWidget::sliceColor(id).rgb();
    int pixels = 0;
    for (int y = 0; y < 180; ++y) {
        for (int x = rightSide ? 400 : 0; x < (rightSide ? 800 : 400); ++x) {
            if (image.pixel(x, y) == colour) { ++pixels; }
        }
    }
    return pixels;
}

// The shift a slice's demodulator needs: from its own stream's centre.
double ownStreamShift(RadioModel* model, SliceModel* slice)
{
    return slice->frequency() - model->streamCentreHz(slice->streamIndex());
}

class TstDesktopStationWindow final : public QObject {
    Q_OBJECT
private slots:
    void hostedDashboardClearsWhenDesktopLosesLastReceiver()
    {
        if (!QSslSocket::supportsSsl()) { QSKIP("Qt reports no working TLS backend."); }
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("station.settings")));
        MainWindow window({}, nullptr, MainWindow::ConnectionStartup::Deferred);
        RadioModel* model = window.radioModel();
        model->setBoardForTest(HPSDRHW::Saturn);
        model->configureStreamPool(5, 5, 192000);
        model->setConnectionStateForTest(ConnectionState::Connected);
        const int aId = model->addSlice(QStringLiteral("pan-0"));
        const int bId = model->addSlice(QStringLiteral("pan-0"));
        SliceModel* a = model->sliceById(aId);
        SliceModel* b = model->sliceById(bId);
        QVERIFY(a && b);
        a->setDspMode(DSPMode::CWU);
        DesktopStationController controller(model, optionsFor(settings, directory.path()));
        window.setDesktopStationController(&controller);
        QVERIFY(controller.start(true));
        SliceOwnership* ownership = model->sliceOwnership();
        ownership->setOwner(bId, QByteArrayLiteral("token:phone"));
        auto* dashboard = window.findChild<RxDashboard*>();
        QVERIFY(dashboard);
        QCOMPARE(dashboard->slice(), a);
        QCOMPARE(dashboard->modeText(), QStringLiteral("CWU"));

        ownership->setOwner(aId, QByteArrayLiteral("token:phone"));
        // Slice control plan Task 3: the former controller stays a listener,
        // so the bottom bar follows A as a listened slice (ruling U7).
        QVERIFY(ownership->isListening(SliceOwnership::stationDevice(), aId));
        QCOMPARE(dashboard->slice(), a);
        // It clears once the desktop neither controls nor listens to a slice.
        QVERIFY(ownership->leave(SliceOwnership::stationDevice(), aId));
        QVERIFY(ownership->leave(SliceOwnership::stationDevice(), bId));
        QCOMPARE(dashboard->slice(), nullptr);
        QCOMPARE(dashboard->modeText(), QStringLiteral("–"));
        QVERIFY(dashboard->sliceLetter().isNull());

        ownership->setOwner(bId, SliceOwnership::stationDevice());
        b->setDspMode(DSPMode::AM);
        QCOMPARE(dashboard->slice(), b);
        QCOMPARE(dashboard->sliceLetter(), QLatin1Char('B'));
        QCOMPARE(dashboard->modeText(), QStringLiteral("AM"));
    }

    void hostedSetupEditsOnlyDesktopReceiver()
    {
        if (!QSslSocket::supportsSsl()) { QSKIP("Qt reports no working TLS backend."); }
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("station.settings")));
        MainWindow window({}, nullptr, MainWindow::ConnectionStartup::Deferred);
        RadioModel* model = window.radioModel();
        model->setBoardForTest(HPSDRHW::Saturn);
        model->configureStreamPool(5, 5, 192000);
        model->setConnectionStateForTest(ConnectionState::Connected);
        const int aId = model->addSlice(QStringLiteral("pan-0"));
        const int bId = model->addSlice(QStringLiteral("pan-0"));
        const int cId = model->addSlice(QStringLiteral("pan-0"));
        SliceModel* a = model->sliceById(aId);
        SliceModel* b = model->sliceById(bId);
        SliceModel* c = model->sliceById(cId);
        QVERIFY(a && b && c);
        DesktopStationController controller(model, optionsFor(settings, directory.path()));
        window.setDesktopStationController(&controller);
        QVERIFY(controller.start(true));
        SliceOwnership* ownership = model->sliceOwnership();
        ownership->setOwner(bId, QByteArrayLiteral("token:phone"));
        QVERIFY(model->setActiveSliceByIdFor(QByteArrayLiteral("token:phone"), bId));
        model->setTransmitHolder(QByteArrayLiteral("token:phone"));
        QCOMPARE(model->activeSlice(), b);
        QCOMPARE(ownership->activeFor(SliceOwnership::stationDevice()), aId);

        SetupDialog* dialog = nullptr;
        connect(&window, &MainWindow::setupDialogCreated, &window,
                [&dialog](SetupDialog* created) { dialog = created; });
        QVERIFY(QMetaObject::invokeMethod(&window, "createSetupDialog"));
        QVERIFY(dialog);
        dialog->selectPage(QStringLiteral("AGC/ALC"));
        auto* agc = dialog->findChild<AgcAlcSetupPage*>();
        QVERIFY(agc);
        auto attack = [agc] {
            for (QSpinBox* spin : agc->findChildren<QSpinBox*>()) {
                if (spin->property("nereusSetupId").toString() == QStringLiteral("dsp.agcAlc.agcAttack")) {
                    return spin;
                }
            }
            return static_cast<QSpinBox*>(nullptr);
        };
        QVERIFY(attack());
        attack()->setValue(37);
        QCOMPARE(a->agcAttack(), 37);
        QVERIFY(b->agcAttack() != 37);

        QVERIFY(model->setActiveSliceByIdFor(SliceOwnership::stationDevice(), cId));
        QCOMPARE(model->activeSlice(), b);
        QVERIFY(attack());
        attack()->setValue(53);
        QCOMPARE(c->agcAttack(), 53);
        QCOMPARE(a->agcAttack(), 37);
        QVERIFY(b->agcAttack() != 53);

        dialog->selectPage(QStringLiteral("TNF"));
        auto* tnf = dialog->findChild<MnfSetupPage*>();
        QVERIFY(tnf);
        auto* add = tnf->findChild<QPushButton*>(QStringLiteral("btnMNFAdd"));
        QVERIFY(add);
        c->setFrequency(14074000.0);
        b->setFrequency(7100000.0);
        add->click();
        QCOMPARE(model->notchModel()->notches().size(), 1);
        QCOMPARE(model->notchModel()->notches().first().centerHz, c->frequency());
        dialog->close();

        // A flag shortcut names C explicitly. It must first select C for
        // this desktop, even while B remains the Core's active receiver.
        QVERIFY(model->setActiveSliceByIdFor(SliceOwnership::stationDevice(), aId));
        QCOMPARE(model->activeSlice(), b);
        VfoWidget* cFlag = flagFor(window, cId);
        QVERIFY(cFlag);
        dialog = nullptr;
        QVERIFY(QMetaObject::invokeMethod(cFlag, "openSetupRequested"));
        QCOMPARE(ownership->activeFor(SliceOwnership::stationDevice()), cId);
        QVERIFY(dialog);
        auto* flagAgc = dialog->findChild<AgcAlcSetupPage*>();
        QVERIFY(flagAgc);
        QSpinBox* flagAttack = nullptr;
        for (QSpinBox* spin : flagAgc->findChildren<QSpinBox*>()) {
            if (spin->property("nereusSetupId").toString()
                == QStringLiteral("dsp.agcAlc.agcAttack")) {
                flagAttack = spin;
                break;
            }
        }
        QVERIFY(flagAttack);
        flagAttack->setValue(69);
        QCOMPARE(c->agcAttack(), 69);
        QVERIFY(b->agcAttack() != 69);

        QPointer<QSpinBox> oldFlagAttack(flagAttack);
        ownership->setOwner(cId, QByteArrayLiteral("token:phone"));
        ownership->setOwner(aId, QByteArrayLiteral("token:phone"));
        QCOMPARE(ownership->activeFor(SliceOwnership::stationDevice()), -1);
        QVERIFY(oldFlagAttack.isNull());
        dialog->selectPage(QStringLiteral("TNF"));
        auto* emptyTnf = dialog->findChild<MnfSetupPage*>();
        QVERIFY(emptyTnf);
        auto* disabledAdd = emptyTnf->findChild<QPushButton*>(QStringLiteral("btnMNFAdd"));
        QVERIFY(disabledAdd);
        QVERIFY(!disabledAdd->isEnabled());
        disabledAdd->click();
        QCOMPARE(model->notchModel()->notches().size(), 1);
        dialog->close();
    }

    void foreignGlobalActiveNeverBecomesDesktopTarget()
    {
        if (!QSslSocket::supportsSsl()) { QSKIP("Qt reports no working TLS backend."); }
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("station.settings")));
        MainWindow window({}, nullptr, MainWindow::ConnectionStartup::Deferred);
        RadioModel* model = window.radioModel();
        model->setBoardForTest(HPSDRHW::Saturn);
        model->configureStreamPool(5, 5, 192000);
        model->setConnectionStateForTest(ConnectionState::Connected);
        const int aId = model->addSlice(QStringLiteral("pan-0"));
        const int bId = model->addSlice(QStringLiteral("pan-0"));
        const int cId = model->addSlice(QStringLiteral("pan-0"));
        SliceModel* a = model->sliceById(aId);
        SliceModel* b = model->sliceById(bId);
        SliceModel* c = model->sliceById(cId);
        QVERIFY(a && b && c);
        DesktopStationController controller(model, optionsFor(settings, directory.path()));
        window.setDesktopStationController(&controller);
        QVERIFY(controller.start(true));
        SliceOwnership* ownership = model->sliceOwnership();
        ownership->setOwner(bId, QByteArrayLiteral("token:phone"));
        QVERIFY(model->setActiveSliceByIdFor(QByteArrayLiteral("token:phone"), bId));
        model->setTransmitHolder(QByteArrayLiteral("token:phone"));
        QCOMPARE(model->activeSlice(), b);
        QCOMPARE(ownership->activeFor(SliceOwnership::stationDevice()), aId);

        QMenu* dsp = menuNamed(window, QStringLiteral("DSP"));
        QVERIFY(dsp);
        QAction* anf = actionNamed(dsp, QStringLiteral("ANF"));
        QVERIFY(anf);
        anf->trigger();
        QVERIFY(a->anfEnabled());
        QVERIFY(!b->anfEnabled());
        QAction* nbMenuAction = actionNamed(dsp, QStringLiteral("NB"));
        QVERIFY(nbMenuAction);
        QMenu* nb = nbMenuAction->menu();
        QVERIFY(nb);
        QAction* nbAction = actionNamed(nb, QStringLiteral("NB"));
        QVERIFY(nbAction);
        nbAction->trigger();
        QCOMPARE(a->nbMode(), NbMode::NB);
        // Co-hosted slices intentionally share the stream's single NB state.
        QCOMPARE(b->nbMode(), NbMode::NB);
        QMenu* mode = menuNamed(window, QStringLiteral("Mode"));
        QVERIFY(mode);
        QAction* am = actionNamed(mode, QStringLiteral("AM"));
        QVERIFY(am);
        const DSPMode foreignMode = b->dspMode();
        am->trigger();
        QCOMPARE(a->dspMode(), DSPMode::AM);
        QCOMPARE(b->dspMode(), foreignMode);

        auto* dashboard = window.findChild<RxDashboard*>();
        QVERIFY(dashboard);
        QCOMPARE(dashboard->sliceLetter(), a->sliceLetter());
        auto* pans = window.findChild<PanadapterStack*>();
        QVERIFY(pans);
        auto* pan = pans->panadapter(QStringLiteral("pan-0"));
        QVERIFY(pan);
        QCOMPARE(pan->activeSliceIndex(), aId);
        pan->setActiveSliceIndex(bId);  // stale foreign pan selection
        const double foreignHz = b->frequency();
        const double tunedHz = a->frequency() + 1000.0;
        QVERIFY(QMetaObject::invokeMethod(pan->spectrumWidget(), "frequencyClicked",
            Qt::DirectConnection, Q_ARG(double, tunedHz)));
        QCOMPARE(a->frequency(), tunedHz);
        QCOMPARE(b->frequency(), foreignHz);

        QVERIFY(model->setActiveSliceByIdFor(SliceOwnership::stationDevice(), cId));
        QCOMPARE(model->activeSlice(), b);
        QCOMPARE(dashboard->sliceLetter(), c->sliceLetter());
        QCOMPARE(pan->activeSliceIndex(), cId);
        anf->trigger();
        QVERIFY(c->anfEnabled());
        QVERIFY(!b->anfEnabled());

        ContainerWidget* container = window.findChild<ContainerWidget*>();
        QVERIFY(container);
        container->setRxSource(bId + 1);
        QVERIFY(QMetaObject::invokeMethod(container, "otherButtonClicked", Qt::DirectConnection,
            Q_ARG(int, int(OtherButtonItem::ButtonId::Snb))));
        QVERIFY(!b->snbEnabled());
    }

    void hostModeOwnFlagsAndDetach()
    {
        if (!QSslSocket::supportsSsl()) { QSKIP("Qt reports no working TLS backend."); }
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("station.settings")));
        MainWindow window({}, nullptr, MainWindow::ConnectionStartup::Deferred);
        RadioModel* model = window.radioModel();
        model->setBoardForTest(HPSDRHW::Saturn);
        model->configureStreamPool(5, 5, 192000);
        model->setConnectionStateForTest(ConnectionState::Connected);
        const int aId = model->addSlice(QStringLiteral("pan-0"));
        const int bId = model->addSlice(QStringLiteral("pan-0"));
        SliceModel* a = model->sliceById(aId);
        SliceModel* b = model->sliceById(bId);
        QVERIFY(a && b);
        VfoWidget* flagA = flagFor(window, a->sliceIndex());
        VfoWidget* flagB = flagFor(window, b->sliceIndex());
        QVERIFY(flagA && flagB);

        DesktopStationController controller(model, optionsFor(settings, directory.path()));
        window.setDesktopStationController(&controller);
        QVERIFY(controller.start(true));
        auto* tci = window.findChild<TciServer*>();
        QVERIFY(tci);
        QVERIFY(tci->desktopHostMode());
        SliceOwnership* ownership = model->sliceOwnership();
        QCOMPARE(ownership->activeFor(SliceOwnership::stationDevice()), a->sliceIndex());
        QCOMPARE(controller.server()->takeTransmitForStation({}, {}),
                 TransmitHolder::TakeVerdict::AtOnce);
        window.refreshDesktopStationState();
        QVERIFY(flagA->txSliceShown());

        ownership->setOwner(b->sliceIndex(), QByteArrayLiteral("token:phone"));
        // The former controller stays a listener (Task 3); leaving makes B
        // wholly the phone's, shown as its marker.
        QVERIFY(ownership->leave(SliceOwnership::stationDevice(), b->sliceIndex()));
        QVERIFY(flagA->stationPresentationAllowed());
        QVERIFY(!flagB->stationPresentationAllowed());
        QVERIFY(!flagB->txSliceShown());
        SpectrumWidget* spectrum = qobject_cast<SpectrumWidget*>(flagB->parentWidget());
        QVERIFY(spectrum);
        QCOMPARE(spectrum->foreignSliceMarkers().size(), 1);
        spectrum->updateVfoPositions();
        QVERIFY(flagB->isHidden());
        QVERIFY(model->setActiveSliceByIdFor(SliceOwnership::stationDevice(),
                                             a->sliceIndex()));
        QCOMPARE(ownership->activeFor(SliceOwnership::stationDevice()), a->sliceIndex());
        QVERIFY(!model->setActiveSliceByIdFor(SliceOwnership::stationDevice(),
                                              b->sliceIndex()));

        window.setDesktopStationController(nullptr);
        QVERIFY(!controller.enabled());
        QVERIFY(!tci->desktopHostMode());
        QVERIFY(flagB->stationPresentationAllowed());
        QVERIFY(spectrum->foreignSliceMarkers().isEmpty());
        auto replacement = std::make_unique<DesktopStationController>(
            model, optionsFor(settings, directory.path()));
        window.setDesktopStationController(replacement.get());
        QVERIFY(replacement->start(true));
        QVERIFY(tci->desktopHostMode());
        QCOMPARE(spectrum->foreignSliceMarkers().size(), 1);
        replacement.reset();
        QVERIFY(!tci->desktopHostMode());
        QVERIFY(flagB->stationPresentationAllowed());
        QVERIFY(spectrum->foreignSliceMarkers().isEmpty());
        SetupDialog* observed = nullptr;
        connect(&window, &MainWindow::setupDialogCreated, &window,
                [&observed](SetupDialog* dialog) { observed = dialog; });
        QVERIFY(QMetaObject::invokeMethod(&window, "createSetupDialog"));
        QVERIFY(observed);
        observed->close();
    }

    void hostShowsOtherDeviceAsForeignMarker()
    {
        if (!QSslSocket::supportsSsl()) { QSKIP("Qt reports no working TLS backend."); }
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("station.settings")));
        MainWindow window({}, nullptr, MainWindow::ConnectionStartup::Deferred);
        RadioModel* model = window.radioModel();
        model->setBoardForTest(HPSDRHW::Saturn);
        model->configureStreamPool(5, 5, 192000);
        model->setConnectionStateForTest(ConnectionState::Connected);
        const int aId = model->addSlice(QStringLiteral("pan-0"));
        const int bId = model->addSlice(QStringLiteral("pan-0"));
        SliceModel* a = model->sliceById(aId);
        SliceModel* b = model->sliceById(bId);
        QVERIFY(a && b);
        DesktopStationController controller(model, optionsFor(settings, directory.path()));
        window.setDesktopStationController(&controller);
        QVERIFY(controller.start(true));
        StationServer* server = controller.server();
        QVERIFY(server);
        QObject phoneSession;
        DeviceSessionRegistry::Entry phone;
        phone.deviceId = QByteArrayLiteral("phone-device-id-for-away-state-01");
        phone.kind = DeviceSessionRegistry::Kind::Paired;
        phone.name = QStringLiteral("Living room iPhone");
        phone.shortName = QStringLiteral("iPhone");
        phone.deviceKind = QStringLiteral("phone");
        QCOMPARE(server->deviceSessions()->admit(phone, &phoneSession).admission,
                 DeviceSessionRegistry::Admission::Admitted);
        auto* spectrum = window.findChild<PanadapterStack*>()->panadapter(
            QStringLiteral("pan-0"))->spectrumWidget();
        QVERIFY(spectrum);
        SliceOwnership* ownership = model->sliceOwnership();
        ownership->setOwner(bId, phone.deviceId);
        // The former controller stays a listener (Task 3); leaving makes B
        // wholly the phone's, shown as its marker.
        QVERIFY(ownership->leave(SliceOwnership::stationDevice(), bId));
        QVERIFY(flagFor(window, aId)->stationPresentationAllowed());
        QVERIFY(!flagFor(window, bId)->stationPresentationAllowed());
        QCOMPARE(spectrum->sliceMarkerGeometry().size(), 1);
        QCOMPARE(spectrum->sliceMarkerGeometry().first().flag,
                 static_cast<const VfoWidget*>(flagFor(window, aId)));
        QCOMPARE(spectrum->foreignSliceMarkers().size(), 1);
        const auto first = spectrum->foreignSliceMarkers().first();
        QCOMPARE(first.sliceId, bId);
        QCOMPARE(first.letter, QStringLiteral("B"));
        QCOMPARE(first.ownerShortName, QStringLiteral("iPhone"));
        QCOMPARE(first.color, VfoWidget::sliceColor(bId));
        QCOMPARE(SpectrumWidget::foreignMarkerLabel(first), QStringLiteral("B iPhone"));

        TransmitHolder::KeyRequest key;
        key.deviceId = phone.deviceId;
        QCOMPARE(server->transmitHolder()->askKey(key).verdict, KeyingVerdict::Admit);
        b->setTxSlice(true);
        QVERIFY(b->txSliceMarked());
        QVERIFY(spectrum->foreignSliceMarkers().first().tx);
        QCOMPARE(SpectrumWidget::foreignMarkerLabel(spectrum->foreignSliceMarkers().first()),
                 QStringLiteral("B iPhone TX"));
        server->transmitHolder()->release(phone.deviceId, QStringLiteral("test release"));
        QVERIFY(!spectrum->foreignSliceMarkers().first().tx);

        auto* extra = window.findChild<PanadapterStack*>()->addPanadapter(
            QStringLiteral("pan-1"));
        QVERIFY(extra && extra->spectrumWidget());
        QCOMPARE(extra->spectrumWidget()->foreignSliceMarkers().size(), 1);

        b->setFrequency(b->frequency() + 1200.0);
        b->setFilter(150, 2700);
        QCOMPARE(spectrum->foreignSliceMarkers().first().centreHz, b->frequency());
        QCOMPARE(spectrum->foreignSliceMarkers().first().filterLowHz, 150);
        QCOMPARE(spectrum->foreignSliceMarkers().first().filterHighHz, 2700);
        server->deviceSessions()->sessionEnded(phone.deviceId, &phoneSession,
            DeviceSessionRegistry::EndKind::Dropped);
        QVERIFY(spectrum->foreignSliceMarkers().first().away);

        ownership->setOwner(bId, SliceOwnership::stationDevice());
        QVERIFY(spectrum->foreignSliceMarkers().isEmpty());
        QCOMPARE(spectrum->sliceMarkerGeometry().size(), 2);
        ownership->hold(bId, phone.deviceId);
        QCOMPARE(spectrum->foreignSliceMarkers().size(), 1);
        QVERIFY(spectrum->foreignSliceMarkers().first().away);
        QCOMPARE(spectrum->sliceMarkerGeometry().size(), 1);
        ownership->setOwner(bId, phone.deviceId);
        // Still a listener: B keeps a read-only flag, not a marker.
        QVERIFY(spectrum->foreignSliceMarkers().isEmpty());
        QCOMPARE(spectrum->sliceMarkerGeometry().size(), 2);
        QVERIFY(flagFor(window, bId)->isListening());
        QVERIFY(ownership->leave(SliceOwnership::stationDevice(), bId));
        QCOMPARE(spectrum->foreignSliceMarkers().size(), 1);

        ownership->setOwner(aId, phone.deviceId);
        QVERIFY(ownership->leave(SliceOwnership::stationDevice(), aId));
        QVERIFY(spectrum->sliceMarkerGeometry().isEmpty());
        QCOMPARE(spectrum->foreignSliceMarkers().size(), 2);
        model->removeSlice(bId);
        QCOMPARE(spectrum->foreignSliceMarkers().size(), 1);
        controller.stop();
        QVERIFY(spectrum->foreignSliceMarkers().isEmpty());
    }

    // Take-over re-review (N-3): the hosting window's controlTaken card
    // stays when Take it back is refused and may be tried again (the slice
    // transmitting), and goes when the take-back works.
    void hostTakeItBackCardStaysWhileItMayBeTriedAgain()
    {
        if (!QSslSocket::supportsSsl()) { QSKIP("Qt reports no working TLS backend."); }
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("station.settings")));
        MainWindow window({}, nullptr, MainWindow::ConnectionStartup::Deferred);
        RadioModel* model = window.radioModel();
        model->setBoardForTest(HPSDRHW::Saturn);
        model->configureStreamPool(5, 5, 192000);
        model->setConnectionStateForTest(ConnectionState::Connected);
        const int aId = model->addSlice(QStringLiteral("pan-0"));
        DesktopStationController controller(model, optionsFor(settings, directory.path()));
        window.setDesktopStationController(&controller);
        QVERIFY(controller.start(true));
        StationServer* server = controller.server();
        QVERIFY(server);
        QObject phoneSession;
        const DeviceSessionRegistry::Entry phone =
            admitPhone(*server, phoneSession, QByteArrayLiteral("phone-device-id-for-take-back-01"));
        QVERIFY(!phone.deviceId.isEmpty());
        SliceOwnership* ownership = model->sliceOwnership();
        const QByteArray station = SliceOwnership::stationDevice();
        QCOMPARE(ownership->mark(aId).owner, station);

        const auto cards = [&window]() {
            QList<NoticeCard*> shown;
            for (NoticeCard* card : window.findChildren<NoticeCard*>()) {
                if (card->isVisibleTo(&window) && card->takeBackButton() != nullptr) {
                    shown.append(card);
                }
            }
            return shown;
        };
        SliceAccessController* access = server->sliceAccessController();
        QVERIFY(access);
        const SliceOwnership::SliceRef ref{aId, ownership->incarnation(aId)};
        QVERIFY(access->listen(phone.deviceId, ref).accepted);
        const SliceAccessController::Result took =
            access->takeControl(phone.deviceId, ref, ownership->controlRevision(aId));
        QVERIFY2(took.accepted, qPrintable(took.reason));
        QCOMPARE(ownership->mark(aId).owner, phone.deviceId);
        QTRY_COMPARE(cards().size(), 1);
        NoticeCard* card = cards().first();
        QVERIFY(card->takeBackButton()->isEnabled());

        // The phone keys on the slice it took: Take it back is refused and
        // may be tried again, so the card stays.
        TransmitHolder::KeyRequest key;
        key.deviceId = phone.deviceId;
        QCOMPARE(server->transmitHolder()->askKey(key).verdict, KeyingVerdict::Admit);
        server->transmitHolder()->setKeyed(true);
        TxSliceArbiter* arbiter = model->txSliceArbiter();
        if (arbiter->txBoundSliceId() != aId) {
            QVERIFY(arbiter->requestHandoff(aId, phone.deviceId));
        }
        QVERIFY(server->sliceOnAir(aId));
        card->takeBackButton()->click();
        QTest::qWait(100);
        QCOMPARE(ownership->mark(aId).owner, phone.deviceId);
        QCOMPARE(cards().size(), 1);

        // Once it stops, the same tap works (JJ's wider ruling, 2026-09-30:
        // every slice can be taken, and the one refusal is while it
        // transmits). The phone, a session with no link here, cannot stay
        // on as a listener, so it loses the slice; the card goes.
        server->transmitHolder()->release(phone.deviceId, QStringLiteral("test release"));
        QVERIFY(!server->sliceOnAir(aId));
        cards().first()->takeBackButton()->click();
        QTRY_COMPARE(ownership->mark(aId).owner, station);
        QVERIFY(!ownership->listenersOf(aId).contains(phone.deviceId));
        QTRY_COMPARE(cards().size(), 0);
        controller.stop();
    }

    // Review fix (Job B, c): once the phone lets go of the slice, control
    // moved on. The host's Take it back can never run now: it is refused,
    // the card goes, ownership does not change, and asked again the Core
    // answers "That can no longer be taken back.".
    void hostTakeItBackCardGoesAfterThePhoneReleases()
    {
        if (!QSslSocket::supportsSsl()) { QSKIP("Qt reports no working TLS backend."); }
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("station.settings")));
        MainWindow window({}, nullptr, MainWindow::ConnectionStartup::Deferred);
        RadioModel* model = window.radioModel();
        model->setBoardForTest(HPSDRHW::Saturn);
        model->configureStreamPool(5, 5, 192000);
        model->setConnectionStateForTest(ConnectionState::Connected);
        const int aId = model->addSlice(QStringLiteral("pan-0"));
        DesktopStationController controller(model, optionsFor(settings, directory.path()));
        window.setDesktopStationController(&controller);
        QVERIFY(controller.start(true));
        StationServer* server = controller.server();
        QVERIFY(server);
        QObject phoneSession;
        const DeviceSessionRegistry::Entry phone =
            admitPhone(*server, phoneSession, QByteArrayLiteral("phone-device-id-for-take-back-02"));
        QVERIFY(!phone.deviceId.isEmpty());
        SliceOwnership* ownership = model->sliceOwnership();
        const QByteArray station = SliceOwnership::stationDevice();
        QCOMPARE(ownership->mark(aId).owner, station);

        const auto cards = [&window]() {
            QList<NoticeCard*> shown;
            for (NoticeCard* card : window.findChildren<NoticeCard*>()) {
                if (card->isVisibleTo(&window) && card->takeBackButton() != nullptr) {
                    shown.append(card);
                }
            }
            return shown;
        };
        SliceAccessController* access = server->sliceAccessController();
        QVERIFY(access);
        const SliceOwnership::SliceRef ref{aId, ownership->incarnation(aId)};
        QVERIFY(access->listen(phone.deviceId, ref).accepted);
        const SliceAccessController::Result took =
            access->takeControl(phone.deviceId, ref, ownership->controlRevision(aId));
        QVERIFY2(took.accepted, qPrintable(took.reason));
        QCOMPARE(ownership->mark(aId).owner, phone.deviceId);
        QTRY_COMPARE(cards().size(), 1);
        NoticeCard* card = cards().first();
        QVERIFY(card->takeBackButton()->isEnabled());

        const SliceAccessController::Result released =
            access->release(phone.deviceId, SliceOwnership::SliceRef{aId, ownership->incarnation(aId)},
                            ownership->controlRevision(aId));
        QVERIFY2(released.accepted, qPrintable(released.reason));
        const QByteArray ownerAfterRelease = ownership->mark(aId).owner;
        QVERIFY(ownerAfterRelease != phone.deviceId);
        const quint64 revisionAfterRelease = ownership->controlRevision(aId);

        HostingSliceActions* actions = window.hostingSliceActionsForTest();
        QVERIFY(actions);
        QSignalSpy answered(actions, &HostingSliceActions::finished);
        QSignalSpy ended(actions, &HostingSliceActions::takeBackAnswered);
        card->takeBackButton()->click();
        QTRY_VERIFY(!answered.isEmpty());
        QList<QVariant> answer = answered.takeFirst();
        QCOMPARE(answer.at(0).toByteArray(), QByteArrayLiteral("notice.takeBack"));
        QVERIFY(!answer.at(2).toBool());
        // Desktop listening lane (JJ, 2026-09-30): the phone released the
        // slice, so the take-back record is void and the first tap says so
        // (it used to answer with slice.takeControl's stale-revision
        // words). The Core forgets the take-back.
        QCOMPARE(answer.at(3).toString(), QStringLiteral("That can no longer be taken back."));
        QTRY_COMPARE(ended.size(), 1);
        QVERIFY(ended.first().at(1).toBool());
        QTRY_COMPARE(cards().size(), 0);
        // Asked again, the Core has nothing left to take back.
        actions->takeBack(ended.first().at(0).toLongLong());
        QTRY_VERIFY(!answered.isEmpty());
        answer = answered.takeFirst();
        QVERIFY(!answer.at(2).toBool());
        QCOMPARE(answer.at(3).toString(), QStringLiteral("That can no longer be taken back."));
        QCOMPARE(ownership->mark(aId).owner, ownerAfterRelease);
        QCOMPARE(ownership->controlRevision(aId), revisionAfterRelease);
        controller.stop();
    }

    // TX rulings review (N-3 through the card): closing a controlTaken
    // card with its close button forgets its Take it back record.
    void closingTheHostCardForgetsItsTakeBack()
    {
        if (!QSslSocket::supportsSsl()) { QSKIP("Qt reports no working TLS backend."); }
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("station.settings")));
        MainWindow window({}, nullptr, MainWindow::ConnectionStartup::Deferred);
        RadioModel* model = window.radioModel();
        model->setBoardForTest(HPSDRHW::Saturn);
        model->configureStreamPool(5, 5, 192000);
        model->setConnectionStateForTest(ConnectionState::Connected);
        const int aId = model->addSlice(QStringLiteral("pan-0"));
        DesktopStationController controller(model, optionsFor(settings, directory.path()));
        window.setDesktopStationController(&controller);
        QVERIFY(controller.start(true));
        StationServer* server = controller.server();
        QVERIFY(server);
        QObject phoneSession;
        const DeviceSessionRegistry::Entry phone =
            admitPhone(*server, phoneSession, QByteArrayLiteral("phone-device-id-for-card-close-1"));
        QVERIFY(!phone.deviceId.isEmpty());
        SliceOwnership* ownership = model->sliceOwnership();

        const auto cards = [&window]() {
            QList<NoticeCard*> found;
            for (NoticeCard* card : window.findChildren<NoticeCard*>()) {
                if (card->isVisibleTo(&window) && card->takeBackButton() != nullptr) {
                    found.append(card);
                }
            }
            return found;
        };
        HostingSliceActions* actions = window.hostingSliceActionsForTest();
        QVERIFY(actions);
        qint64 noticeId = 0;
        connect(actions, &HostingSliceActions::notice, &window,
                [&noticeId](const SessionMessage& notice) { noticeId = notice.prompt.id; });
        SliceAccessController* access = server->sliceAccessController();
        const SliceOwnership::SliceRef ref{aId, ownership->incarnation(aId)};
        QVERIFY(access->listen(phone.deviceId, ref).accepted);
        QVERIFY(access->takeControl(phone.deviceId, ref, ownership->controlRevision(aId)).accepted);
        QTRY_COMPARE(cards().size(), 1);
        QVERIFY(noticeId != 0);
        QVERIFY(actions->hasTakeBackForTest(noticeId));

        NoticeCard* card = cards().first();
        QVERIFY(card->closeButton());
        card->closeButton()->click();
        QTRY_COMPARE(cards().size(), 0);
        QVERIFY(!actions->hasTakeBackForTest(noticeId));
        controller.stop();
    }

    // Slice control plan Task 14a: a slice the hosting window listens to
    // keeps its flag, says who controls it, never writes the slice, and its
    // Stop listening leaves the slice.
    void hostListensToAnotherDevicesSlice()
    {
        if (!QSslSocket::supportsSsl()) { QSKIP("Qt reports no working TLS backend."); }
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("station.settings")));
        MainWindow window({}, nullptr, MainWindow::ConnectionStartup::Deferred);
        RadioModel* model = window.radioModel();
        model->setBoardForTest(HPSDRHW::Saturn);
        model->configureStreamPool(5, 5, 192000);
        model->setConnectionStateForTest(ConnectionState::Connected);
        const int aId = model->addSlice(QStringLiteral("pan-0"));
        const int bId = model->addSlice(QStringLiteral("pan-0"));
        SliceModel* b = model->sliceById(bId);
        QVERIFY(b);
        DesktopStationController controller(model, optionsFor(settings, directory.path()));
        window.setDesktopStationController(&controller);
        QVERIFY(controller.start(true));
        StationServer* server = controller.server();
        QVERIFY(server);
        QObject phoneSession;
        DeviceSessionRegistry::Entry phone;
        phone.deviceId = QByteArrayLiteral("phone-device-id-for-listening-01");
        phone.kind = DeviceSessionRegistry::Kind::Paired;
        phone.name = QStringLiteral("Living room iPhone");
        phone.shortName = QStringLiteral("iPhone");
        phone.deviceKind = QStringLiteral("phone");
        QCOMPARE(server->deviceSessions()->admit(phone, &phoneSession).admission,
                 DeviceSessionRegistry::Admission::Admitted);
        auto* spectrum = window.findChild<PanadapterStack*>()->panadapter(
            QStringLiteral("pan-0"))->spectrumWidget();
        QVERIFY(spectrum);
        SliceOwnership* ownership = model->sliceOwnership();
        ownership->setOwner(bId, phone.deviceId);
        VfoWidget* flagA = flagFor(window, aId);
        VfoWidget* flagB = flagFor(window, bId);
        QVERIFY(flagA && flagB);
        // The phone took the slice; this window, its former controller,
        // stays a listener.
        QVERIFY(ownership->isListening(SliceOwnership::stationDevice(), bId));
        QVERIFY(flagB->stationPresentationAllowed());
        QVERIFY(flagB->isListening());
        QVERIFY(flagB->accessLineText().startsWith(QStringLiteral("Listening")));
        QVERIFY(flagB->accessLineText().contains(QStringLiteral("iPhone")));
        QCOMPARE(flagA->sliceAccess().state, VfoWidget::SliceAccess::State::Controlled);
        QCOMPARE(flagA->accessLineText(), QStringLiteral("You control"));
        QVERIFY(spectrum->foreignSliceMarkers().isEmpty());
        QCOMPARE(spectrum->sliceMarkerGeometry().size(), 2);

        // The listener never writes the slice.
        const double before = b->frequency();
        QSignalSpy freq(b, &SliceModel::frequencyChanged);
        QSignalSpy af(b, &SliceModel::afGainChanged);
        QSignalSpy mute(b, &SliceModel::mutedChanged);
        const QPointF at(flagB->width() / 2.0, flagB->height() / 2.0);
        QWheelEvent wheel(at, flagB->mapToGlobal(at), QPoint(), QPoint(0, 120), Qt::NoButton,
                          Qt::NoModifier, Qt::NoScrollPhase, false);
        QApplication::sendEvent(flagB, &wheel);
        QCOMPARE(freq.count(), 0);
        QCOMPARE(af.count(), 0);
        QCOMPARE(mute.count(), 0);
        QCOMPARE(b->frequency(), before);

        // The phone keys on B, then moves transmit to its other slice C.
        // Only the TX slice changed, and C is on the air, so C's listened
        // flag turns red and B's clears; this window's own A stays clear.
        const int cId = model->addSlice(QStringLiteral("pan-0"));
        ownership->setOwner(cId, phone.deviceId);
        VfoWidget* flagC = flagFor(window, cId);
        QVERIFY(flagC);
        QVERIFY(flagC->isListening());
        TxSliceArbiter* arbiter = model->txSliceArbiter();
        QVERIFY(arbiter);
        TransmitHolder::KeyRequest key;
        key.deviceId = phone.deviceId;
        QCOMPARE(server->transmitHolder()->askKey(key).verdict, KeyingVerdict::Admit);
        server->transmitHolder()->setKeyed(true);
        if (arbiter->txBoundSliceId() != bId) {
            QVERIFY(arbiter->requestHandoff(bId, phone.deviceId));
        }
        QCOMPARE(arbiter->txBoundSliceId(), bId);
        QVERIFY(server->sliceOnAir(bId));
        QVERIFY(flagB->txSliceShown());
        QVERIFY(!flagC->txSliceShown());
        QVERIFY(arbiter->requestHandoff(cId, phone.deviceId));
        QCOMPARE(arbiter->txBoundSliceId(), cId);
        QVERIFY(server->sliceOnAir(cId));
        QVERIFY(flagC->txSliceShown());
        QVERIFY(!flagB->txSliceShown());
        QVERIFY(!flagA->txSliceShown());
        server->transmitHolder()->release(phone.deviceId, QStringLiteral("test release"));
        QVERIFY(!server->sliceOnAir(cId));
        QVERIFY(!flagC->txSliceShown());
        model->removeSlice(cId);

        // Stop listening from the flag: the Core answers, nothing is left
        // waiting, and the slice is the phone's marker again.
        emit flagB->stopListeningRequested(bId);
        QVERIFY(!ownership->isListening(SliceOwnership::stationDevice(), bId));
        QVERIFY(!flagB->sliceAccessPending());
        QVERIFY(!flagB->stationPresentationAllowed());
        QCOMPARE(spectrum->foreignSliceMarkers().size(), 1);

        // Listening again brings the read-only flag back.
        QVERIFY(ownership->join(SliceOwnership::stationDevice(), bId));
        QVERIFY(flagB->stationPresentationAllowed());
        QVERIFY(flagB->isListening());
        QVERIFY(spectrum->foreignSliceMarkers().isEmpty());
        controller.stop();
    }

    // Slice control plan Task 15 (rulings U5, U6, U7): the RX applet has a
    // tab for the slice this window listens to, saying who controls it;
    // selecting it moves this window's RX (the applet, the bottom bar and
    // the flag focus) and never the active slice, the TX slice or the
    // holder; the applet holds its shared controls with the controller
    // named; Stop listening from the applet leaves the slice.
    void hostRxAppletFollowsAListenedTab()
    {
        if (!QSslSocket::supportsSsl()) { QSKIP("Qt reports no working TLS backend."); }
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("station.settings")));
        MainWindow window({}, nullptr, MainWindow::ConnectionStartup::Deferred);
        RadioModel* model = window.radioModel();
        model->setBoardForTest(HPSDRHW::Saturn);
        model->configureStreamPool(5, 5, 192000);
        model->setConnectionStateForTest(ConnectionState::Connected);
        const int aId = model->addSlice(QStringLiteral("pan-0"));
        const int bId = model->addSlice(QStringLiteral("pan-0"));
        SliceModel* a = model->sliceById(aId);
        SliceModel* b = model->sliceById(bId);
        QVERIFY(a && b);
        DesktopStationController controller(model, optionsFor(settings, directory.path()));
        window.setDesktopStationController(&controller);
        QVERIFY(controller.start(true));
        StationServer* server = controller.server();
        QVERIFY(server);
        QObject phoneSession;
        DeviceSessionRegistry::Entry phone;
        phone.deviceId = QByteArrayLiteral("phone-device-id-for-rx-applet-01");
        phone.kind = DeviceSessionRegistry::Kind::Paired;
        phone.name = QStringLiteral("Living room iPhone");
        phone.shortName = QStringLiteral("iPhone");
        phone.deviceKind = QStringLiteral("phone");
        QCOMPARE(server->deviceSessions()->admit(phone, &phoneSession).admission,
                 DeviceSessionRegistry::Admission::Admitted);
        SliceOwnership* ownership = model->sliceOwnership();
        const QByteArray station = SliceOwnership::stationDevice();
        ownership->setOwner(bId, phone.deviceId);
        QVERIFY(ownership->isListening(station, bId));

        RxApplet* applet = window.findChild<RxApplet*>();
        QVERIFY(applet);
        QToolButton* tabA = sliceTabFor(*applet, QLatin1Char('A'));
        QToolButton* tabB = sliceTabFor(*applet, QLatin1Char('B'));
        QVERIFY(tabA && tabB);
        QVERIFY(tabB->isEnabled());
        QVERIFY(tabB->toolTip().contains(QStringLiteral("Listening")));
        QVERIFY(tabB->toolTip().contains(QStringLiteral("iPhone")));
        QCOMPARE(applet->slice(), a);
        QVERIFY(!applet->isListening());

        const int activeBefore = ownership->activeFor(station);
        QCOMPARE(activeBefore, aId);
        TxSliceArbiter* arbiter = model->txSliceArbiter();
        QVERIFY(arbiter);
        const int txBefore = arbiter->txBoundSliceId();
        const TransmitHolder::State holderBefore = server->transmitHolder()->state();
        QSignalSpy freq(b, &SliceModel::frequencyChanged);
        QSignalSpy mode(b, &SliceModel::dspModeChanged);
        QSignalSpy af(b, &SliceModel::afGainChanged);
        QSignalSpy mute(b, &SliceModel::mutedChanged);

        tabB->click();
        QCOMPARE(ownership->activeRxFor(station), bId);
        QCOMPARE(ownership->activeFor(station), activeBefore);
        QCOMPARE(arbiter->txBoundSliceId(), txBefore);
        QCOMPARE(server->transmitHolder()->state(), holderBefore);
        QCOMPARE(applet->slice(), b);
        QVERIFY(applet->isListening());
        QVERIFY(applet->sliceAccess().heldReason.contains(QStringLiteral("iPhone")));
        RxDashboard* dashboard = window.findChild<RxDashboard*>();
        QVERIFY(dashboard);
        QCOMPARE(dashboard->slice(), b);
        PanadapterApplet* pan = window.findChild<PanadapterStack*>()->panadapter(
            QStringLiteral("pan-0"));
        QVERIFY(pan);
        QCOMPARE(pan->activeSliceIndex(), bId);
        QCOMPARE(freq.count(), 0);
        QCOMPARE(mode.count(), 0);
        QCOMPARE(af.count(), 0);
        QCOMPARE(mute.count(), 0);

        // Back to the slice this window controls.
        tabA->click();
        QCOMPARE(ownership->activeRxFor(station), aId);
        QCOMPARE(applet->slice(), a);
        QVERIFY(!applet->isListening());
        QCOMPARE(dashboard->slice(), a);

        // Stop listening from the applet: the Core answers, nothing is left
        // waiting, and the applet keeps the slice this window controls.
        tabB->click();
        QCOMPARE(applet->slice(), b);
        emit applet->stopListeningRequested(bId);
        QVERIFY(!ownership->isListening(station, bId));
        QCOMPARE(applet->slice(), a);
        QVERIFY(!applet->isListening());
        QCOMPARE(dashboard->slice(), a);
        controller.stop();
    }

    // Slice control plan Task 15 fix round 1: a container set to a slice
    // this window listens to refuses Mute (and the other slice buttons)
    // with the RX applet's reason, naming the device that controls it.
    void hostContainerOnAListenedSliceRefusesMute()
    {
        if (!QSslSocket::supportsSsl()) { QSKIP("Qt reports no working TLS backend."); }
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("station.settings")));
        MainWindow window({}, nullptr, MainWindow::ConnectionStartup::Deferred);
        RadioModel* model = window.radioModel();
        model->setBoardForTest(HPSDRHW::Saturn);
        model->configureStreamPool(5, 5, 192000);
        model->setConnectionStateForTest(ConnectionState::Connected);
        const int aId = model->addSlice(QStringLiteral("pan-0"));
        const int bId = model->addSlice(QStringLiteral("pan-0"));
        SliceModel* a = model->sliceById(aId);
        SliceModel* b = model->sliceById(bId);
        QVERIFY(a && b);
        DesktopStationController controller(model, optionsFor(settings, directory.path()));
        window.setDesktopStationController(&controller);
        QVERIFY(controller.start(true));
        StationServer* server = controller.server();
        QVERIFY(server);
        QObject phoneSession;
        DeviceSessionRegistry::Entry phone;
        phone.deviceId = QByteArrayLiteral("phone-device-id-for-rx-applet-01");
        phone.kind = DeviceSessionRegistry::Kind::Paired;
        phone.name = QStringLiteral("Living room iPhone");
        phone.shortName = QStringLiteral("iPhone");
        phone.deviceKind = QStringLiteral("phone");
        QCOMPARE(server->deviceSessions()->admit(phone, &phoneSession).admission,
                 DeviceSessionRegistry::Admission::Admitted);
        auto* manager = window.findChild<ContainerManager*>();
        QVERIFY(manager);
        ContainerWidget* container =
            manager->createContainer(bId + 1, DockMode::Floating);  // slice B
        auto* meter = new MeterWidget();
        container->setContent(meter);
        auto* buttons = new OtherButtonItem();
        meter->addItem(buttons);
        container->wireInteractiveItem(buttons);
        const auto destroy = qScopeGuard([manager, container] {
            manager->destroyContainer(container->id());
        });

        // The button follows the change of control with no other refresh.
        using Id = OtherButtonItem::ButtonId;
        QVERIFY(buttons->isButtonAvailable(Id::Mute));
        SliceOwnership* ownership = model->sliceOwnership();
        const QByteArray station = SliceOwnership::stationDevice();
        ownership->setOwner(bId, phone.deviceId);
        QVERIFY(ownership->isListening(station, bId));
        const bool mutedBefore = b->muted();
        QSignalSpy muted(b, &SliceModel::mutedChanged);
        QVERIFY(!buttons->isButtonAvailable(Id::Mute));
        const QString reason = buttons->buttonUnavailableReason(buttons->indexOf(Id::Mute));
        QCOMPARE(reason, QStringLiteral("Living room iPhone controls this slice"));
        emit container->otherButtonClicked(int(Id::Mute));
        QCOMPARE(b->muted(), mutedBefore);
        QCOMPARE(muted.count(), 0);

        ownership->setOwner(bId, station);
        QVERIFY(buttons->isButtonAvailable(Id::Mute));
        controller.stop();
    }

    // Slice control plan Task 15 fix round 1: without the hosting slice
    // requests (the fallback path), a tab click still reaches a listened
    // slice as this window's RX without moving its active slice, and
    // reaches the slice it controls.
    void hostTabFallbackReachesAListenedSlice()
    {
        if (!QSslSocket::supportsSsl()) { QSKIP("Qt reports no working TLS backend."); }
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("station.settings")));
        MainWindow window({}, nullptr, MainWindow::ConnectionStartup::Deferred);
        RadioModel* model = window.radioModel();
        model->setBoardForTest(HPSDRHW::Saturn);
        model->configureStreamPool(5, 5, 192000);
        model->setConnectionStateForTest(ConnectionState::Connected);
        const int aId = model->addSlice(QStringLiteral("pan-0"));
        const int bId = model->addSlice(QStringLiteral("pan-0"));
        SliceModel* a = model->sliceById(aId);
        SliceModel* b = model->sliceById(bId);
        QVERIFY(a && b);
        DesktopStationController controller(model, optionsFor(settings, directory.path()));
        window.setDesktopStationController(&controller);
        QVERIFY(controller.start(true));
        StationServer* server = controller.server();
        QVERIFY(server);
        QObject phoneSession;
        DeviceSessionRegistry::Entry phone;
        phone.deviceId = QByteArrayLiteral("phone-device-id-for-rx-applet-01");
        phone.kind = DeviceSessionRegistry::Kind::Paired;
        phone.name = QStringLiteral("Living room iPhone");
        phone.shortName = QStringLiteral("iPhone");
        phone.deviceKind = QStringLiteral("phone");
        QCOMPARE(server->deviceSessions()->admit(phone, &phoneSession).admission,
                 DeviceSessionRegistry::Admission::Admitted);
        SliceOwnership* ownership = model->sliceOwnership();
        const QByteArray station = SliceOwnership::stationDevice();
        ownership->setOwner(bId, phone.deviceId);
        QVERIFY(ownership->isListening(station, bId));

        window.dropHostingSliceActionsForTest();
        RxApplet* applet = window.findChild<RxApplet*>();
        QVERIFY(applet);
        QToolButton* tabA = sliceTabFor(*applet, QLatin1Char('A'));
        QToolButton* tabB = sliceTabFor(*applet, QLatin1Char('B'));
        QVERIFY(tabA && tabB);
        const int activeBefore = ownership->activeFor(station);
        QCOMPARE(activeBefore, aId);

        tabB->click();
        QCOMPARE(ownership->activeRxFor(station), bId);
        QCOMPARE(ownership->activeFor(station), activeBefore);
        QCOMPARE(applet->slice(), b);
        QVERIFY(applet->isListening());

        tabA->click();
        QCOMPARE(ownership->activeRxFor(station), aId);
        QCOMPARE(ownership->activeFor(station), aId);
        QCOMPARE(applet->slice(), a);
        QVERIFY(!applet->isListening());
        controller.stop();
    }

    void appletAndContainerAskWithoutOptimisticKey()
    {
        if (!QSslSocket::supportsSsl()) { QSKIP("Qt reports no working TLS backend."); }
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("station.settings")));
        MainWindow window({}, nullptr, MainWindow::ConnectionStartup::Deferred);
        RadioModel* model = window.radioModel();
        model->setBoardForTest(HPSDRHW::Saturn);
        model->configureStreamPool(5, 5, 192000);
        model->setConnectionStateForTest(ConnectionState::Connected);
        QVERIFY(model->addSlice(QStringLiteral("pan-0")) >= 0);
        DesktopStationController controller(model, optionsFor(settings, directory.path()));
        window.setDesktopStationController(&controller);
        QVERIFY(controller.start(true));
        TxApplet* tx = window.findChild<TxApplet*>();
        QVERIFY(tx && tx->moxButton() && tx->tuneButton());
        TransmitHolder* holder = controller.server()->transmitHolder();
        tx->tuneButton()->click();
        QTRY_VERIFY(holder->isHeldBy(SliceOwnership::stationDevice()));
        QVERIFY(!tx->tuneButton()->isChecked() || model->isTune());
        controller.requestTune(false);
        QTRY_VERIFY(!model->isTune());
        holder->release(SliceOwnership::stationDevice(), QStringLiteral("test hand-back"));
        QTRY_VERIFY(holder->state() == TransmitHolder::State::Unheld);
        QObject peerSession;
        DeviceSessionRegistry::Entry peer;
        peer.deviceId = QByteArrayLiteral("token:phone");
        peer.kind = DeviceSessionRegistry::Kind::Token;
        peer.name = QStringLiteral("Phone");
        peer.shortName = QStringLiteral("Phone");
        peer.deviceKind = QStringLiteral("phone");
        StationServer* server = controller.server();
        QCOMPARE(server->deviceSessions()->admit(peer, &peerSession).admission,
                 DeviceSessionRegistry::Admission::Admitted);
        TransmitHolder::KeyRequest key;
        key.deviceId = peer.deviceId;
        QCOMPARE(holder->askKey(key).verdict, KeyingVerdict::Admit);
        tx->moxButton()->click();
        TakeTransmitDialog* question = window.findChild<TakeTransmitDialog*>();
        QVERIFY(question);
        QCOMPARE(question->questionLabel()->text(), QStringLiteral("Take transmit from Phone?"));
        QVERIFY(!tx->moxButton()->isChecked());
        QVERIFY(holder->isHeldBy(peer.deviceId));
        QPointer<TakeTransmitDialog> cancelled(question);
        question->cancelButton()->click();
        QTRY_VERIFY(cancelled.isNull());
        QVERIFY(!model->mox());

        holder->setKeyed(true);
        tx->moxButton()->click();
        question = window.findChild<TakeTransmitDialog*>();
        QVERIFY(question);
        QVERIFY(question->redButton());
        QCOMPARE(question->takeButton()->text(), QStringLiteral("Unkey and take over"));
        QVERIFY(!tx->moxButton()->isChecked());
        QPointer<TakeTransmitDialog> keyedQuestion(question);
        question->cancelButton()->click();
        QTRY_VERIFY(keyedQuestion.isNull());
        holder->setKeyed(false);

        ContainerWidget* container = window.findChild<ContainerWidget*>();
        QVERIFY(container);
        QVERIFY(QMetaObject::invokeMethod(container, "otherButtonClicked", Qt::DirectConnection,
            Q_ARG(int, int(OtherButtonItem::ButtonId::Tun))));
        question = window.findChild<TakeTransmitDialog*>();
        QVERIFY(question);
        QVERIFY(!tx->tuneButton()->isChecked());
        QVERIFY(holder->isHeldBy(peer.deviceId));
        QPointer<TakeTransmitDialog> confirmed(question);
        question->takeButton()->click();
        QTRY_VERIFY(confirmed.isNull());
        QTRY_VERIFY(holder->isHeldBy(SliceOwnership::stationDevice()));
        QVERIFY(!tx->tuneButton()->isChecked() || model->isTune());
        controller.requestTune(false);
        QTRY_VERIFY(!model->isTune());
        holder->release(SliceOwnership::stationDevice(), QStringLiteral("test hand-back"));
        QTRY_VERIFY(holder->state() == TransmitHolder::State::Unheld);
        QCOMPARE(holder->askKey(key).verdict, KeyingVerdict::Admit);
        tx->moxButton()->click();
        question = window.findChild<TakeTransmitDialog*>();
        QVERIFY(question);
        QPointer<TakeTransmitDialog> pending(question);
        controller.stop();
        QVERIFY(!pending || !pending->isVisible());
        QVERIFY(!controller.enabled());
        QCOMPARE(tx->tuneButton()->isChecked(), model->isTune());
    }

    // Fix wave (hosting 2-TONE parity): while another device holds
    // transmit, the hosting window's 2-TONE asks to take it, as its MOX and
    // TUNE do, from the TX applet and from a container. Nothing keys until
    // the question is answered.
    void hostTwoToneAsksTheTakeQuestion()
    {
        if (!QSslSocket::supportsSsl()) { QSKIP("Qt reports no working TLS backend."); }
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("station.settings")));
        MainWindow window({}, nullptr, MainWindow::ConnectionStartup::Deferred);
        RadioModel* model = window.radioModel();
        model->setBoardForTest(HPSDRHW::Saturn);
        model->configureStreamPool(5, 5, 192000);
        model->setConnectionStateForTest(ConnectionState::Connected);
        QVERIFY(model->addSlice(QStringLiteral("pan-0")) >= 0);
        DesktopStationController controller(model, optionsFor(settings, directory.path()));
        window.setDesktopStationController(&controller);
        QVERIFY(controller.start(true));
        TxApplet* tx = window.findChild<TxApplet*>();
        QVERIFY(tx && tx->twoToneButton());
        TwoToneController* twoTone = model->twoToneController();
        QVERIFY(twoTone);
        // Fix round 2 (minor 2): a recording TX channel, so the start is
        // seen (twoToneActiveChanged and the generator's run), not read
        // from a log line. No radio is connected; nothing leaves the test.
        ToneRecordingTxChannel tone;
        twoTone->setTxChannel(&tone);
        twoTone->setSliceModel(model->activeSlice());
        twoTone->setSettleDelaysMs(0, 0);
        twoTone->setPowerOn(true);
        QSignalSpy activeChanged(twoTone, &TwoToneController::twoToneActiveChanged);
        TransmitHolder* holder = controller.server()->transmitHolder();
        QObject peerSession;
        DeviceSessionRegistry::Entry peer;
        peer.deviceId = QByteArrayLiteral("token:phone");
        peer.kind = DeviceSessionRegistry::Kind::Token;
        peer.name = QStringLiteral("Phone");
        peer.shortName = QStringLiteral("Phone");
        peer.deviceKind = QStringLiteral("phone");
        QCOMPARE(controller.server()->deviceSessions()->admit(peer, &peerSession).admission,
                 DeviceSessionRegistry::Admission::Admitted);
        TransmitHolder::KeyRequest key;
        key.deviceId = peer.deviceId;
        QCOMPARE(holder->askKey(key).verdict, KeyingVerdict::Admit);

        // The TX applet's 2-TONE asks.
        tx->twoToneButton()->click();
        TakeTransmitDialog* question = window.findChild<TakeTransmitDialog*>();
        QVERIFY(question);
        QCOMPARE(question->questionLabel()->text(), QStringLiteral("Take transmit from Phone?"));
        QCOMPARE(question->detailLabel()->text(),
                 QStringLiteral("The other device holds transmit. Take it to start the "
                                "2-tone test."));
        QVERIFY(!tx->twoToneButton()->isChecked());
        QVERIFY(!twoTone->isActive());
        QVERIFY(holder->isHeldBy(peer.deviceId));
        QPointer<TakeTransmitDialog> cancelled(question);
        question->cancelButton()->click();
        QTRY_VERIFY(cancelled.isNull());
        QVERIFY(!twoTone->isActive());
        QVERIFY(holder->isHeldBy(peer.deviceId));
        QVERIFY(!tx->twoToneButton()->isChecked());

        // The container's 2TONE asks the same question; taking it moves
        // transmit to this computer.
        ContainerWidget* container = window.findChild<ContainerWidget*>();
        QVERIFY(container);
        QVERIFY(QMetaObject::invokeMethod(container, "otherButtonClicked", Qt::DirectConnection,
            Q_ARG(int, int(OtherButtonItem::ButtonId::TwoTon))));
        question = window.findChild<TakeTransmitDialog*>();
        QVERIFY(question);
        QVERIFY(!twoTone->isActive());
        QVERIFY(holder->isHeldBy(peer.deviceId));
        QPointer<TakeTransmitDialog> taken(question);
        // Nothing started while the question was open or cancelled.
        QVERIFY(activeChanged.isEmpty());
        QVERIFY(!tone.runs.contains(true));
        // With transmit taken, the test is started for this computer.
        question->takeButton()->click();
        QTRY_VERIFY(taken.isNull());
        QTRY_VERIFY(holder->isHeldBy(SliceOwnership::stationDevice()));
        QTRY_VERIFY(twoTone->isActive());
        QCOMPARE(activeChanged.size(), 1);
        QCOMPARE(activeChanged.first().first().toBool(), true);
        QVERIFY(tone.runs.contains(true));
        model->setTwoTone(false);
        QTRY_VERIFY(!twoTone->isActive());
        QCOMPARE(tone.runs.last(), false);
        twoTone->setTxChannel(nullptr);
        controller.stop();
    }

    // Station VOX (whole-branch review, TX path): while another device holds
    // transmit, the hosting window's VOX is disabled naming the holder, as a
    // remote window's is; VOX armed here is dropped when that device takes
    // transmit; with transmit unheld or held here, VOX is live again.
    void hostVoxFollowsTheTransmitHolder()
    {
        if (!QSslSocket::supportsSsl()) { QSKIP("Qt reports no working TLS backend."); }
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("station.settings")));
        MainWindow window({}, nullptr, MainWindow::ConnectionStartup::Deferred);
        RadioModel* model = window.radioModel();
        model->setBoardForTest(HPSDRHW::Saturn);
        model->configureStreamPool(5, 5, 192000);
        model->setConnectionStateForTest(ConnectionState::Connected);
        QVERIFY(model->addSlice(QStringLiteral("pan-0")) >= 0);
        DesktopStationController controller(model, optionsFor(settings, directory.path()));
        window.setDesktopStationController(&controller);
        QVERIFY(controller.start(true));
        TxApplet* tx = window.findChild<TxApplet*>();
        QVERIFY(tx && tx->voxButton());
        QPushButton* vox = tx->voxButton();
        TransmitModel& transmit = model->transmitModel();
        TransmitHolder* holder = controller.server()->transmitHolder();
        QCOMPARE(holder->state(), TransmitHolder::State::Unheld);
        QVERIFY(vox->isEnabled());

        // VOX armed here, then a phone takes transmit: VOX is dropped.
        vox->click();
        QVERIFY(transmit.voxEnabled());
        QObject peerSession;
        DeviceSessionRegistry::Entry peer;
        peer.deviceId = QByteArrayLiteral("token:phone");
        peer.kind = DeviceSessionRegistry::Kind::Token;
        peer.name = QStringLiteral("Phone");
        peer.shortName = QStringLiteral("Phone");
        peer.deviceKind = QStringLiteral("phone");
        StationServer* server = controller.server();
        QCOMPARE(server->deviceSessions()->admit(peer, &peerSession).admission,
                 DeviceSessionRegistry::Admission::Admitted);
        TransmitHolder::KeyRequest key;
        key.deviceId = peer.deviceId;
        QCOMPARE(holder->askKey(key).verdict, KeyingVerdict::Admit);
        QVERIFY(holder->isHeldBy(peer.deviceId));
        QVERIFY(!transmit.voxEnabled());
        QVERIFY(!vox->isChecked());

        // While the phone holds transmit: disabled, the holder named.
        const QString reason = TxRefusals::otherDeviceHolds(QStringLiteral("Phone")).text;
        QTRY_VERIFY(!vox->isEnabled());
        QCOMPARE(vox->toolTip(), reason);
        QCOMPARE(vox->accessibleDescription(), reason);
        vox->click();
        QVERIFY(!transmit.voxEnabled());

        // The phone lets go: VOX is live here again.
        holder->release(peer.deviceId, QStringLiteral("test hand-back"));
        QTRY_VERIFY(holder->state() == TransmitHolder::State::Unheld);
        QTRY_VERIFY(vox->isEnabled());
        QVERIFY(vox->toolTip() != reason);
        vox->click();
        QVERIFY(transmit.voxEnabled());
        vox->click();
        QVERIFY(!transmit.voxEnabled());
        controller.stop();
    }

    // Desktop listening lane (JJ, 2026-09-30; it replaces Task 16's
    // auto-stop): listening ends only when the operator ends it. A layout
    // change that takes away the pan showing a slice this window listens to
    // (another device controls it) places the slice on a pan that remains
    // and keeps listening, with no stopListening and no notice. Nothing
    // about the slice changes: no slice is added or removed, and its
    // stream, its receiver, its tuning and its pan for the controller all
    // stay. A slice this window controls on the retired pan rehomes as
    // before.
    void layoutChangeKeepsListeningToASliceOnARemainingPan()
    {
        if (!QSslSocket::supportsSsl()) { QSKIP("Qt reports no working TLS backend."); }
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("station.settings")));
        MainWindow window({}, nullptr, MainWindow::ConnectionStartup::Deferred);
        RadioModel* model = window.radioModel();
        model->setBoardForTest(HPSDRHW::Saturn);
        model->configureStreamPool(5, 5, 192000);
        model->setConnectionStateForTest(ConnectionState::Connected);
        const int aId = model->addSlice(QStringLiteral("pan-0"));
        const int bId = model->addSlice(QStringLiteral("pan-1"));
        QVERIFY(applyLayout(window, QStringLiteral("2v")));
        auto* stack = window.findChild<PanadapterStack*>();
        QVERIFY(stack);
        QCOMPARE(stack->currentLayoutId(), QStringLiteral("2v"));
        QCOMPARE(model->slices().size(), 2);
        // A slice this window controls, on the pan the layout retires.
        const int cId = model->addSlice(QStringLiteral("pan-1"));
        QVERIFY(cId >= 0);
        QCOMPARE(model->slices().size(), 3);
        SliceModel* b = model->sliceById(bId);
        QVERIFY(b);
        // B sits well away from A, so the pan that remains cannot show it.
        b->setFrequency(model->sliceById(aId)->frequency() + 1'000'000.0);
        DesktopStationController controller(model, optionsFor(settings, directory.path()));
        window.setDesktopStationController(&controller);
        QVERIFY(controller.start(true));
        StationServer* server = controller.server();
        QVERIFY(server);
        QObject phoneSession;
        const DeviceSessionRegistry::Entry phone =
            admitPhone(*server, phoneSession, QByteArrayLiteral("phone-device-id-for-layouts-0001"));
        QVERIFY(!phone.deviceId.isEmpty());
        SliceOwnership* ownership = model->sliceOwnership();
        const QByteArray station = SliceOwnership::stationDevice();
        ownership->setOwner(bId, phone.deviceId);
        QVERIFY(ownership->isListening(station, bId));
        QVERIFY(flagFor(window, bId) && flagFor(window, bId)->isListening());
        QCOMPARE(ownership->mark(cId).owner, station);

        QHash<int, int> ddcs;
        QHash<int, int> streams;
        for (SliceModel* slice : model->slices()) {
            if (slice->sliceIndex() == cId) { continue; }
            streams.insert(slice->sliceIndex(), slice->streamIndex());
            ddcs.insert(slice->sliceIndex(), model->ddcForStream(slice->streamIndex()));
        }
        const QList<QByteArray> listenersOfB = ownership->listenersOf(bId);
        QSignalSpy bListeners(ownership, &SliceOwnership::listenersChanged);
        const int bDdc = b->ddcIndex();
        const double frequency = b->frequency();
        QSignalSpy added(model, &RadioModel::sliceAdded);
        QSignalSpy removed(model, &RadioModel::sliceRemoved);
        QSignalSpy tuned(b, &SliceModel::frequencyChanged);
        QSignalSpy moved(b, &SliceModel::panKeyChanged);

        SpectrumWidget* remaining = stack->spectrum(QStringLiteral("pan-0"));
        QVERIFY(remaining);
        const double viewCentre = remaining->centerFrequency();
        const double viewSpan = remaining->bandwidth();
        QVERIFY(applyLayout(window, QStringLiteral("1")));
        QCOMPARE(stack->currentLayoutId(), QStringLiteral("1"));
        // Still listening: no stopListening went out, and no notice.
        QVERIFY(ownership->isListening(station, bId));
        QCOMPARE(ownership->listenersOf(bId), listenersOfB);
        for (const QList<QVariant>& change : std::as_const(bListeners)) {
            QVERIFY(change.first().toInt() != bId);
        }
        QCOMPARE(ownership->mark(bId).subject(), phone.deviceId);
        QCOMPARE(toastsSaying(window, QStringLiteral(
                     "Stopped listening to Slice B: it is no longer shown in this window.")), 0);
        for (StatusToast* toast : window.findChildren<StatusToast*>()) {
            QVERIFY2(!toast->message().startsWith(QStringLiteral("Stopped listening")),
                     qPrintable(toast->message()));
        }
        QCOMPARE(added.count(), 0);
        QCOMPARE(removed.count(), 0);
        QCOMPARE(model->slices().size(), 3);
        for (SliceModel* slice : model->slices()) {
            if (slice->sliceIndex() == cId) { continue; }
            QCOMPARE(slice->streamIndex(), streams.value(slice->sliceIndex()));
            QCOMPARE(model->ddcForStream(slice->streamIndex()), ddcs.value(slice->sliceIndex()));
        }
        QCOMPARE(b->ddcIndex(), bDdc);
        QCOMPARE(b->frequency(), frequency);
        QCOMPARE(tuned.count(), 0);
        // Its pan for its controller stays; this window places it.
        QCOMPARE(moved.count(), 0);
        QCOMPARE(b->panKey(), QStringLiteral("pan-1"));
        QCOMPARE(model->sliceById(aId)->panKey(), QStringLiteral("pan-0"));
        // The controlled slice on the retired pan rehomes as before.
        QCOMPARE(model->sliceById(cId)->panKey(), QStringLiteral("pan-0"));
        // B is shown on the pan that remains, listening, as its flag.
        VfoWidget* flagB = flagFor(window, bId);
        QVERIFY(flagB);
        QVERIFY(flagB->stationPresentationAllowed());
        QVERIFY(flagB->isListening());
        QCOMPARE(flagB->parentWidget(), stack->spectrum(QStringLiteral("pan-0")));
        QCOMPARE(flagCountFor(window, bId), 1);
        QVERIFY(stack->panadapter(QStringLiteral("pan-0"))->associatedSlices().contains(bId));
        // The operator's view of the pan that remains stays where it was.
        QCOMPARE(stack->spectrum(QStringLiteral("pan-0")), remaining);
        QCOMPARE(remaining->centerFrequency(), viewCentre);
        QCOMPARE(remaining->bandwidth(), viewSpan);
        // The operator's own slices stay in view.
        for (const int own : {aId, cId}) {
            const double hz = model->sliceById(own)->frequency();
            QVERIFY(hz >= viewCentre - viewSpan / 2.0 && hz <= viewCentre + viewSpan / 2.0);
            VfoWidget* ownFlag = nullptr;
            for (VfoWidget* flag : remaining->findChildren<VfoWidget*>()) {
                if (flag->sliceIndex() == own) { ownFlag = flag; }
            }
            QVERIFY2(ownFlag, qPrintable(QString::number(own)));
            QVERIFY2(!ownFlag->isHidden(), qPrintable(QString::number(own)));
        }
        // B is off that pan's span: its flag hides and the pan's edge
        // marker points at it, in B's colour, on the right.
        QVERIFY(b->frequency() > remaining->centerFrequency() + remaining->bandwidth() / 2.0);
        QVERIFY(flagB->isHidden());
        const auto edgeMarkerPixels = [remaining, bId](bool rightSide) {
            QImage image(800, 400, QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::black);
            {
                QPainter painter(&image);
                remaining->drawOffScreenIndicatorForTest(painter, QRect(0, 0, 800, 180),
                                                         QRect(0, 200, 800, 180));
            }
            const QRgb bColour = VfoWidget::sliceColor(bId).rgb();
            int pixels = 0;
            for (int y = 0; y < 180; ++y) {
                for (int x = rightSide ? 400 : 0; x < (rightSide ? 800 : 400); ++x) {
                    if (image.pixel(x, y) == bColour) { ++pixels; }
                }
            }
            return pixels;
        };
        QVERIFY2(edgeMarkerPixels(true) > 0, "no edge marker in slice B's colour on the right edge");
        QCOMPARE(edgeMarkerPixels(false), 0);
        // Still listening while it is off the span.
        QVERIFY(ownership->isListening(station, bId));
        // Panned to B, it is in span and shows as its flag, with no marker.
        remaining->setFrequencyRange(b->frequency(), viewSpan);
        QVERIFY(!flagB->isHidden());
        QCOMPARE(edgeMarkerPixels(true), 0);
        QCOMPARE(edgeMarkerPixels(false), 0);
        remaining->setFrequencyRange(viewCentre, viewSpan);
        // Its row still offers Stop listening.
        const QList<SliceChooser::Row> rows =
            SliceChooser::rowsForHostingDesktop(*model, *server);
        const auto rowB = std::find_if(rows.cbegin(), rows.cend(),
                                       [bId](const SliceChooser::Row& r) { return r.sliceId == bId; });
        QVERIFY(rowB != rows.cend());
        QVERIFY(rowB->listeningHere);
        QCOMPARE(rowB->controller, SliceChooser::Controller::OtherDevice);
        SliceChooser chooser;
        chooser.setInventory(rows);
        chooser.selectSlice(bId);
        bool stopOffered = false;
        for (QPushButton* action :
             chooser.findChildren<QPushButton*>(QStringLiteral("sliceChooserAction"))) {
            if (action->text() == QStringLiteral("Stop listening") && action->isEnabled()) {
                stopOffered = true;
            }
        }
        QVERIFY(stopOffered);
        controller.stop();
    }

    void aPlacedListenedSliceTunedByItsControllerKeepsItsShiftAndThePansView()
    {
        if (!QSslSocket::supportsSsl()) { QSKIP("Qt reports no working TLS backend."); }
        PlacedListen f;
        prepareListenedSlice(f, true);
        if (QTest::currentTestFailed()) { return; }
        placeListenedSlice(f);
        if (QTest::currentTestFailed()) { return; }
        SliceModel* b = f.model->sliceById(f.bId);
        FFTRouter* router = f.model->fftRouter();
        QVERIFY(router);
        // Its controller tunes it inside its DDC, then well away.
        for (const double step : {5'000.0, 30'000.0, 400'000.0}) {
            b->setFrequency(b->frequency() + step);
            QCOMPARE(b->shiftOffsetHz(), ownStreamShift(f.model, b));
            QCOMPARE(f.remaining->centerFrequency(), f.viewCentre);
            QCOMPARE(f.remaining->bandwidth(), f.viewSpan);
            QVERIFY(!router->pansForReceiver(b->streamIndex()).contains(QStringLiteral("pan-0")));
            QVERIFY(flagOn(f.remaining, f.bId));
            QVERIFY(flagOn(f.remaining, f.bId)->isHidden());
            QVERIFY(edgeMarkerPixels(f.remaining, f.bId, true) > 0);
        }
        // A and C keep their own shifts from their stream.
        for (const int own : {f.aId, f.cId}) {
            SliceModel* slice = f.model->sliceById(own);
            QCOMPARE(slice->shiftOffsetHz(), ownStreamShift(f.model, slice));
        }
    }

    void aPlacedListenedSlicesEdgeMarkerStaysWhileTheOperatorTunes()
    {
        if (!QSslSocket::supportsSsl()) { QSKIP("Qt reports no working TLS backend."); }
        PlacedListen f;
        prepareListenedSlice(f, true);
        if (QTest::currentTestFailed()) { return; }
        placeListenedSlice(f);
        if (QTest::currentTestFailed()) { return; }
        SliceModel* b = f.model->sliceById(f.bId);
        const double bShift = b->shiftOffsetHz();
        for (const int own : {f.aId, f.cId, f.aId}) {
            SliceModel* slice = f.model->sliceById(own);
            slice->setFrequency(slice->frequency() + 2'000.0);
            QVERIFY(flagOn(f.remaining, f.bId)->isHidden());
            QVERIFY2(edgeMarkerPixels(f.remaining, f.bId, true) > 0,
                     qPrintable(QStringLiteral("B's edge marker went after slice %1 tuned")
                                    .arg(own)));
            QCOMPARE(edgeMarkerPixels(f.remaining, f.bId, false), 0);
            QCOMPARE(b->shiftOffsetHz(), bShift);
        }
    }

    void aPlacedListenedSliceNeverScrollsAPanWithCtunOff()
    {
        if (!QSslSocket::supportsSsl()) { QSKIP("Qt reports no working TLS backend."); }
        PlacedListen f;
        prepareListenedSlice(f, false);
        if (QTest::currentTestFailed()) { return; }
        placeListenedSlice(f);
        if (QTest::currentTestFailed()) { return; }
        QVERIFY(!f.remaining->ctunEnabled());
        QCOMPARE(f.remaining->centerFrequency(), f.viewCentre);
        QCOMPARE(f.remaining->bandwidth(), f.viewSpan);
        SliceModel* b = f.model->sliceById(f.bId);
        for (const double step : {5'000.0, 400'000.0}) {
            b->setFrequency(b->frequency() + step);
            QCOMPARE(f.remaining->centerFrequency(), f.viewCentre);
            QCOMPARE(f.remaining->bandwidth(), f.viewSpan);
            QCOMPARE(b->shiftOffsetHz(), ownStreamShift(f.model, b));
            QVERIFY(edgeMarkerPixels(f.remaining, f.bId, true) > 0);
        }
    }

    void aPlacedListenedSliceChangingStreamLeavesNoStaleWindow()
    {
        if (!QSslSocket::supportsSsl()) { QSKIP("Qt reports no working TLS backend."); }
        PlacedListen f;
        prepareListenedSlice(f, true);
        if (QTest::currentTestFailed()) { return; }
        // D shares B's stream and is the phone's too, so B is not alone on
        // its stream and a far tune moves it to another stream index.
        SliceModel* b = f.model->sliceById(f.bId);
        const int dId = f.model->addSlice(QStringLiteral("pan-1"));
        QVERIFY(dId >= 0);
        SliceModel* d = f.model->sliceById(dId);
        d->setFrequency(b->frequency() + 10'000.0);
        QCOMPARE(d->streamIndex(), b->streamIndex());
        f.model->sliceOwnership()->setOwner(dId, f.phone);
        placeListenedSlice(f);
        if (QTest::currentTestFailed()) { return; }
        FFTRouter* router = f.model->fftRouter();
        QVERIFY(router);
        const int before = b->streamIndex();
        b->setFrequency(b->frequency() + 3'000'000.0);
        QVERIFY(b->streamIndex() >= 0);
        QVERIFY(b->streamIndex() != before);
        QCOMPARE(b->shiftOffsetHz(), ownStreamShift(f.model, b));
        QCOMPARE(f.remaining->centerFrequency(), f.viewCentre);
        QCOMPARE(f.remaining->bandwidth(), f.viewSpan);
        for (const int stream : {before, b->streamIndex()}) {
            QVERIFY2(router->pansForReceiver(stream).isEmpty(),
                     qPrintable(QStringLiteral("stream %1 still feeds a pan").arg(stream)));
        }
        QVERIFY(edgeMarkerPixels(f.remaining, f.bId, true) > 0);
    }

    void aRevealStillCentresThePanItOpensOnTheSlice()
    {
        if (!QSslSocket::supportsSsl()) { QSKIP("Qt reports no working TLS backend."); }
        PlacedListen f;
        prepareListenedSlice(f, true);
        if (QTest::currentTestFailed()) { return; }
        placeListenedSlice(f);
        if (QTest::currentTestFailed()) { return; }
        // E: the phone's, on its own stream 2 MHz up, on no pan of this
        // window's.
        const int eId = f.model->addSlice(QStringLiteral("pan-7"));
        QVERIFY(eId >= 0);
        SliceModel* e = f.model->sliceById(eId);
        e->setFrequency(f.model->sliceById(f.aId)->frequency() + 2'000'000.0);
        f.model->sliceOwnership()->setOwner(eId, f.phone);
        QVERIFY(f.model->sliceOwnership()->isListening(SliceOwnership::stationDevice(), eId));
        f.window->revealSliceInWindowForTest(eId);
        // The window grows, and the pan the reveal opens centres on E's
        // stream as it did before this lane.
        QCOMPARE(f.stack->currentLayoutId(), QStringLiteral("2v"));
        SpectrumWidget* opened = f.stack->spectrum(QStringLiteral("pan-1"));
        QVERIFY(opened);
        QVERIFY(f.stack->panadapter(QStringLiteral("pan-1"))->associatedSlices().contains(eId));
        QCOMPARE(opened->centerFrequency(), f.model->streamCentreHz(e->streamIndex()));
        QVERIFY(f.model->fftRouter()->pansForReceiver(e->streamIndex())
                    .contains(QStringLiteral("pan-1")));
        QVERIFY(!opened->isEdgeMarkedSlice(eId));
        // The pan B was placed on is left alone.
        QCOMPARE(f.remaining->centerFrequency(), f.viewCentre);
    }

    void aPlacedListenedSliceIsNeverTheRemainingPansActiveSlice()
    {
        if (!QSslSocket::supportsSsl()) { QSKIP("Qt reports no working TLS backend."); }
        PlacedListen f;
        prepareListenedSlice(f, true);
        if (QTest::currentTestFailed()) { return; }
        // pan-0 is left with no slice listed, so the first slice added to it
        // would be its active slice.
        PanadapterApplet* kept = f.stack->panadapter(QStringLiteral("pan-0"));
        QVERIFY(kept);
        for (int id : kept->associatedSlices()) { kept->removeSlice(id); }
        QVERIFY(kept->associatedSlices().isEmpty());
        QCOMPARE(kept->activeSliceIndex(), -1);
        placeListenedSlice(f);
        if (QTest::currentTestFailed()) { return; }
        QVERIFY(kept->associatedSlices().contains(f.bId));
        QVERIFY(kept->associatedSlices().contains(f.cId));
        QVERIFY(kept->activeSliceIndex() != f.bId);
        QCOMPARE(kept->activeSliceIndex(), f.cId);
    }

    void aListenedSliceWhosePanIsGoneKeepsItsOwnShift()
    {
        if (!QSslSocket::supportsSsl()) { QSKIP("Qt reports no working TLS backend."); }
        PlacedListen f;
        prepareListenedSlice(f, true);
        if (QTest::currentTestFailed()) { return; }
        // B's pan goes without a layout change, so nothing places it and
        // its flag falls back to the active pan.
        f.stack->removePanadapter(QStringLiteral("pan-1"));
        QVERIFY(!f.stack->panadapter(QStringLiteral("pan-1")));
        SliceModel* b = f.model->sliceById(f.bId);
        QCOMPARE(b->panKey(), QStringLiteral("pan-1"));
        // Its DDC holds still (CTUN), so a tune inside it is a shift.
        f.model->setStreamCtunPinned(b->streamIndex(), true);
        QVERIFY(f.model->streamCtunPinned(b->streamIndex()));
        for (const double step : {5'000.0, 30'000.0, 400'000.0}) {
            b->setFrequency(b->frequency() + step);
            QCOMPARE(b->shiftOffsetHz(), ownStreamShift(f.model, b));
        }
    }

    // Slice control plan Task 16 (ruling U1): listening to a slice this
    // window does not show places it in the main window. With one pan and
    // no empty one, the window grows to the next layout and the slice
    // appears on the new pan. No slice is added, and the slice's pan for
    // its controller does not move.
    void listeningToAnUnseenSliceGrowsTheMainWindow()
    {
        if (!QSslSocket::supportsSsl()) { QSKIP("Qt reports no working TLS backend."); }
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("station.settings")));
        MainWindow window({}, nullptr, MainWindow::ConnectionStartup::Deferred);
        RadioModel* model = window.radioModel();
        model->setBoardForTest(HPSDRHW::Saturn);
        model->configureStreamPool(5, 5, 192000);
        model->setConnectionStateForTest(ConnectionState::Connected);
        model->addSlice(QStringLiteral("pan-0"));
        QVERIFY(applyLayout(window, QStringLiteral("1")));
        auto* stack = window.findChild<PanadapterStack*>();
        QVERIFY(stack);
        DesktopStationController controller(model, optionsFor(settings, directory.path()));
        window.setDesktopStationController(&controller);
        QVERIFY(controller.start(true));
        StationServer* server = controller.server();
        QVERIFY(server);
        QObject phoneSession;
        const DeviceSessionRegistry::Entry phone =
            admitPhone(*server, phoneSession, QByteArrayLiteral("phone-device-id-for-layouts-0002"));
        QVERIFY(!phone.deviceId.isEmpty());
        // The phone's slice, on a pan this window does not have.
        const int bId = model->addSlice(QStringLiteral("pan-3"));
        SliceModel* b = model->sliceById(bId);
        QVERIFY(b);
        SliceOwnership* ownership = model->sliceOwnership();
        const QByteArray station = SliceOwnership::stationDevice();
        ownership->setOwner(bId, phone.deviceId);
        ownership->leave(station, bId);
        QVERIFY(!ownership->isListening(station, bId));
        QCOMPARE(stack->currentLayoutId(), QStringLiteral("1"));
        const int count = model->slices().size();
        QSignalSpy added(model, &RadioModel::sliceAdded);
        QSignalSpy moved(b, &SliceModel::panKeyChanged);

        RxDashboard* dashboard = window.findChild<RxDashboard*>();
        QVERIFY(dashboard);
        dashboard->chooserButton()->click();
        SliceChooser* chooser = window.findChild<SliceChooser*>();
        QVERIFY(chooser);
        emit chooser->listenRequested(bId);
        QVERIFY(ownership->isListening(station, bId));
        QCOMPARE(stack->currentLayoutId(), QStringLiteral("2v"));
        PanadapterApplet* grown = stack->panadapter(QStringLiteral("pan-1"));
        QVERIFY(grown);
        QVERIFY(grown->associatedSlices().contains(bId));
        QVERIFY(!stack->panadapter(QStringLiteral("pan-0"))->associatedSlices().contains(bId));
        VfoWidget* flagB = flagFor(window, bId);
        QVERIFY(flagB);
        QCOMPARE(flagB->parentWidget(), grown->spectrumWidget());
        QVERIFY(flagB->stationPresentationAllowed());
        QCOMPARE(flagCountFor(window, bId), 1);
        QCOMPARE(model->slices().size(), count);
        QCOMPARE(added.count(), 0);
        QCOMPARE(moved.count(), 0);
        QCOMPARE(b->panKey(), QStringLiteral("pan-3"));
        QCOMPARE(ownership->mark(bId).subject(), phone.deviceId);
        controller.stop();
    }

    // Slice control plan Task 16 (ruling U2): selecting a slice shown in a
    // floating pan brings that pan forward and makes the slice this
    // window's RX. Nothing moves and no second flag appears.
    void selectingASliceInAFloatingPanBringsItForward()
    {
        if (!QSslSocket::supportsSsl()) { QSKIP("Qt reports no working TLS backend."); }
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("station.settings")));
        MainWindow window({}, nullptr, MainWindow::ConnectionStartup::Deferred);
        RadioModel* model = window.radioModel();
        model->setBoardForTest(HPSDRHW::Saturn);
        model->configureStreamPool(5, 5, 192000);
        model->setConnectionStateForTest(ConnectionState::Connected);
        const int aId = model->addSlice(QStringLiteral("pan-0"));
        const int bId = model->addSlice(QStringLiteral("pan-1"));
        QVERIFY(applyLayout(window, QStringLiteral("2v")));
        auto* stack = window.findChild<PanadapterStack*>();
        QVERIFY(stack);
        DesktopStationController controller(model, optionsFor(settings, directory.path()));
        window.setDesktopStationController(&controller);
        QVERIFY(controller.start(true));
        SliceOwnership* ownership = model->sliceOwnership();
        const QByteArray station = SliceOwnership::stationDevice();
        stack->setActivePan(QStringLiteral("pan-0"));
        stack->floatPanadapter(QStringLiteral("pan-1"));
        QVERIFY(stack->floatingWindowForTest(QStringLiteral("pan-1")));
        // The floating pan put away. (The offscreen platform keeps no
        // stacking order and will not move activation off the floating pan,
        // so a minimized floating pan stands in for one hidden behind other
        // windows.)
        PanFloatingWindow* floater = stack->floatingWindowForTest(QStringLiteral("pan-1"));
        floater->showMinimized();
        QTRY_VERIFY(floater->isMinimized());
        QCOMPARE(ownership->activeRxFor(station), aId);
        SliceModel* b = model->sliceById(bId);
        QSignalSpy moved(b, &SliceModel::panKeyChanged);
        const int count = model->slices().size();

        RxApplet* applet = window.findChild<RxApplet*>();
        QVERIFY(applet);
        QToolButton* tabB = sliceTabFor(*applet, QLatin1Char('B'));
        QVERIFY(tabB);
        tabB->click();
        QCOMPARE(ownership->activeRxFor(station), bId);
        QCOMPARE(stack->activePanId(), QStringLiteral("pan-1"));
        QCOMPARE(stack->floatingWindowForTest(QStringLiteral("pan-1")), floater);
        QVERIFY(floater->isVisible());
        QTRY_VERIFY(!floater->isMinimized());
        QCOMPARE(b->panKey(), QStringLiteral("pan-1"));
        QCOMPARE(moved.count(), 0);
        QCOMPARE(flagCountFor(window, bId), 1);
        QCOMPARE(flagFor(window, bId)->parentWidget(),
                 stack->panadapter(QStringLiteral("pan-1"))->spectrumWidget());
        QCOMPARE(model->slices().size(), count);
        controller.stop();
    }

    // Slice control plan Task 16 (ruling U3): a click on a pan's background
    // gives that pan keyboard and scroll focus only. The slice this window
    // hears and the slice it tunes stay where they were.
    void panBackgroundClickNeverChangesTheSlice()
    {
        if (!QSslSocket::supportsSsl()) { QSKIP("Qt reports no working TLS backend."); }
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("station.settings")));
        MainWindow window({}, nullptr, MainWindow::ConnectionStartup::Deferred);
        RadioModel* model = window.radioModel();
        model->setBoardForTest(HPSDRHW::Saturn);
        model->configureStreamPool(5, 5, 192000);
        model->setConnectionStateForTest(ConnectionState::Connected);
        const int aId = model->addSlice(QStringLiteral("pan-0"));
        model->addSlice(QStringLiteral("pan-1"));
        QVERIFY(applyLayout(window, QStringLiteral("2v")));
        auto* stack = window.findChild<PanadapterStack*>();
        QVERIFY(stack);
        DesktopStationController controller(model, optionsFor(settings, directory.path()));
        window.setDesktopStationController(&controller);
        QVERIFY(controller.start(true));
        SliceOwnership* ownership = model->sliceOwnership();
        const QByteArray station = SliceOwnership::stationDevice();
        QCOMPARE(ownership->activeRxFor(station), aId);
        const int activeBefore = ownership->activeFor(station);
        const int modelActiveBefore = model->activeSlice() ? model->activeSlice()->sliceIndex() : -1;
        PanadapterApplet* pan1 = stack->panadapter(QStringLiteral("pan-1"));
        QVERIFY(pan1);
        emit pan1->activated(QStringLiteral("pan-1"));
        QCOMPARE(ownership->activeRxFor(station), aId);
        QCOMPARE(ownership->activeFor(station), activeBefore);
        QCOMPARE(model->activeSlice() ? model->activeSlice()->sliceIndex() : -1, modelActiveBefore);
        controller.stop();
    }

    // JJ's approved TX-letter take: every controlled letter takes through
    // the flag's confirmation, then selects authoritatively. No key or RX
    // selection occurs, including cancellation and a current refusal.
    void hostTxLettersTakeTransmitThenSelectWithoutKeying()
    {
        if (!QSslSocket::supportsSsl()) { QSKIP("Qt reports no working TLS backend."); }
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("station.settings")));
        MainWindow window({}, nullptr, MainWindow::ConnectionStartup::Deferred);
        RadioModel* model = window.radioModel();
        model->setBoardForTest(HPSDRHW::Saturn);
        model->configureStreamPool(5, 5, 192000);
        model->setConnectionStateForTest(ConnectionState::Connected);
        QList<int> controlled;
        for (int i = 0; i < 4; ++i) {
            const int id = model->addSlice(QStringLiteral("pan-0"));
            QVERIFY(id >= 0);
            controlled.append(id);
        }
        DesktopStationController controller(model, optionsFor(settings, directory.path()));
        window.setDesktopStationController(&controller);
        QVERIFY(controller.start(true));
        StationServer* server = controller.server();
        QVERIFY(server);
        TransmitHolder* holder = server->transmitHolder();
        QObject phoneSession;
        const DeviceSessionRegistry::Entry phone =
            admitPhone(*server, phoneSession, QByteArrayLiteral("phone-device-id-for-tx-letters-01"));
        QVERIFY(!phone.deviceId.isEmpty());
        TransmitHolder::KeyRequest phoneKey;
        phoneKey.deviceId = phone.deviceId;
        QCOMPARE(holder->askKey(phoneKey).verdict, KeyingVerdict::Admit);
        const QByteArray station = SliceOwnership::stationDevice();
        SliceOwnership* ownership = model->sliceOwnership();
        TxSliceArbiter* arbiter = model->txSliceArbiter();
        QVERIFY(arbiter);
        const int boundBefore = arbiter->txBoundSliceId();
        const int target = controlled.last();
        const int activeBefore = model->activeSlice()->sliceIndex();
        const int rxBefore = ownership->activeRxFor(station);
        const auto markBefore = ownership->mark(target);
        bool everKeyed = false;
        QObject keyObserver;
        connect(holder, &TransmitHolder::changed, &keyObserver, [holder, &everKeyed]() {
            everKeyed = everKeyed || (holder->holder() && holder->holder()->keyed);
        });
        QSignalSpy mox(model->moxController(), &MoxController::moxChanged);
        VfoWidget* flag = flagFor(window, target);
        QVERIFY(flag && flag->txBadgeOffer().offered);
        QTRY_VERIFY(txLetterFor(window, target) && txLetterFor(window, target)->isEnabled());
        QCOMPARE(txLetterFor(window, target)->toolTip(), flag->txBadgeOffer().toolTip);

        // Recheck the offer on click, even if the displayed button is old.
        holder->runWithKeyingBlocked([&window, target] { txLetterFor(window, target)->click(); });
        QVERIFY(window.findChild<TakeTransmitDialog*>() == nullptr);
        QCOMPARE(toastsSaying(window, TxRefusals::changingHands().text), 1);
        QVERIFY(holder->isHeldBy(phone.deviceId));
        QCOMPARE(arbiter->txBoundSliceId(), boundBefore);

        txLetterFor(window, target)->click();
        QPointer<TakeTransmitDialog> question = window.findChild<TakeTransmitDialog*>();
        QVERIFY(question);
        QCOMPARE(question->questionLabel()->text(), QStringLiteral("Take transmit from iPhone?"));
        QCOMPARE(txLetterFor(window, target)->isChecked(), boundBefore == target);
        question->cancelButton()->click();
        QTRY_VERIFY(question.isNull());
        QVERIFY(holder->isHeldBy(phone.deviceId));
        QCOMPARE(arbiter->txBoundSliceId(), boundBefore);

        txLetterFor(window, target)->click();
        question = window.findChild<TakeTransmitDialog*>();
        QVERIFY(question);
        question->takeButton()->click();
        QTRY_VERIFY(question.isNull());
        QTRY_VERIFY(holder->isHeldBy(station));
        QTRY_COMPARE(arbiter->txBoundSliceId(), target);
        QTRY_VERIFY(txLetterFor(window, target)->isChecked());
        QCOMPARE(ownership->mark(target).owner, markBefore.owner);

        // All A/B/C/D use the same immediate take when nobody holds TX.
        for (int id : controlled) {
            holder->release(station, QStringLiteral("test hand-back"));
            QTRY_VERIFY(holder->state() == TransmitHolder::State::Unheld);
            QTRY_VERIFY(txLetterFor(window, id) && txLetterFor(window, id)->isEnabled());
            txLetterFor(window, id)->click();
            QVERIFY(window.findChild<TakeTransmitDialog*>() == nullptr);
            QTRY_VERIFY(holder->isHeldBy(station));
            QTRY_COMPARE(arbiter->txBoundSliceId(), id);
            QTRY_VERIFY(txLetterFor(window, id)->isChecked());
        }
        QCOMPARE(model->activeSlice()->sliceIndex(), activeBefore);
        QCOMPARE(ownership->activeRxFor(station), rxBefore);
        QVERIFY(!everKeyed);
        QVERIFY(mox.isEmpty());
        QVERIFY(!model->mox());
        QVERIFY(!model->isTune());
        controller.stop();
    }

    void hostTxLetterPendingTargetMustStillBeTheSameControlledSlice_data()
    {
        QTest::addColumn<QString>("change");
        QTest::addColumn<bool>("viaFlag");
        for (const QString& change : {QStringLiteral("deleted"), QStringLiteral("reused-id"),
                                     QStringLiteral("lost-control"),
                                     QStringLiteral("new-letter-replaces-old")}) {
            QTest::newRow(qPrintable(change)) << change << false;
            QTest::newRow(qPrintable(change + QStringLiteral("-flag"))) << change << true;
        }
    }

    void hostTxLetterPendingTargetMustStillBeTheSameControlledSlice()
    {
        QFETCH(QString, change);
        QFETCH(bool, viaFlag);
        if (!QSslSocket::supportsSsl()) { QSKIP("Qt reports no working TLS backend."); }
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("station.settings")));
        MainWindow window({}, nullptr, MainWindow::ConnectionStartup::Deferred);
        RadioModel* model = window.radioModel();
        model->setBoardForTest(HPSDRHW::Saturn);
        model->configureStreamPool(5, 5, 192000);
        model->setConnectionStateForTest(ConnectionState::Connected);
        const int aId = model->addSlice(QStringLiteral("pan-0"));
        const int bId = model->addSlice(QStringLiteral("pan-0"));
        QVERIFY(aId >= 0 && bId >= 0);
        DesktopStationController controller(model, optionsFor(settings, directory.path()));
        window.setDesktopStationController(&controller);
        QVERIFY(controller.start(true));
        StationServer* server = controller.server();
        QObject phoneSession;
        const DeviceSessionRegistry::Entry phone =
            admitPhone(*server, phoneSession, QByteArrayLiteral("phone-device-id-for-tx-letters-02"));
        QVERIFY(!phone.deviceId.isEmpty());
        TransmitHolder* holder = server->transmitHolder();
        TransmitHolder::KeyRequest key;
        key.deviceId = phone.deviceId;
        QCOMPARE(holder->askKey(key).verdict, KeyingVerdict::Admit);
        QTRY_VERIFY(txLetterFor(window, bId) && txLetterFor(window, bId)->isEnabled());
        if (viaFlag) { flagFor(window, bId)->simulateTxBadgeClick(); }
        else { txLetterFor(window, bId)->click(); }
        QPointer<TakeTransmitDialog> oldQuestion = window.findChild<TakeTransmitDialog*>();
        QVERIFY(oldQuestion);
        const quint64 incarnation = model->sliceOwnership()->incarnation(bId);
        if (change == QLatin1String("new-letter-replaces-old")) {
            txLetterFor(window, aId)->click();
            QTRY_VERIFY(oldQuestion.isNull() || !oldQuestion->isVisible());
        } else if (change == QLatin1String("lost-control")) {
            model->sliceOwnership()->setOwner(bId, phone.deviceId);
            QTRY_VERIFY(txLetterFor(window, bId) == nullptr);
        } else {
            model->removeSlice(bId);
            QVERIFY(model->sliceById(bId) == nullptr);
            if (change == QLatin1String("reused-id")) {
                QCOMPARE(model->addSlice(QStringLiteral("pan-0")), bId);
                QVERIFY(model->sliceOwnership()->incarnation(bId) != incarnation);
            }
        }
        QSignalSpy selected(model, &RadioModel::txSliceSelected);
        QSignalSpy mox(model->moxController(), &MoxController::moxChanged);
        TakeTransmitDialog* question = nullptr;
        for (TakeTransmitDialog* candidate : window.findChildren<TakeTransmitDialog*>()) {
            if (candidate->isVisible()) { question = candidate; }
        }
        if (question) { question->takeButton()->click(); }
        // Whether invalidation cancelled the question or only its queued
        // selection, an unrelated later grant must not revive the old target.
        if (!holder->isHeldBy(SliceOwnership::stationDevice())) {
            holder->release(phone.deviceId, QStringLiteral("test hand-back"));
            QTRY_VERIFY(holder->state() == TransmitHolder::State::Unheld);
            QCOMPARE(controller.requestTakeTransmit().state,
                     DesktopStationController::RequestState::Pending);
        }
        QTRY_VERIFY(holder->isHeldBy(SliceOwnership::stationDevice()));
        bool queueDrained = false;
        QMetaObject::invokeMethod(&window, [&queueDrained]() { queueDrained = true; },
                                  Qt::QueuedConnection);
        QTRY_VERIFY(queueDrained);
        if (change == QLatin1String("new-letter-replaces-old")) {
            QCOMPARE(selected.count(), 1);
            QCOMPARE(selected.first().first().toInt(), aId);
            QCOMPARE(model->txSliceArbiter()->txBoundSliceId(), aId);
        } else {
            QVERIFY(selected.isEmpty());
        }
        QVERIFY(mox.isEmpty());
        QVERIFY(!model->mox());
        QVERIFY(!model->isTune());
        controller.stop();
    }

    // TX badge take (JJ's ruling, 2026-09-30), case 2 on the hosting
    // desktop: the badge of a slice this window controls, while the phone
    // holds transmit, asks "Take transmit from iPhone?" as MOX would, and
    // the accepted take makes the slice the TX slice. A refused take and a
    // cancelled one change nothing. With nobody holding transmit the take
    // is at once. Nothing keys.
    void hostTxBadgeTakesTransmitThenMakesTheSliceTx()
    {
        if (!QSslSocket::supportsSsl()) { QSKIP("Qt reports no working TLS backend."); }
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("station.settings")));
        MainWindow window({}, nullptr, MainWindow::ConnectionStartup::Deferred);
        RadioModel* model = window.radioModel();
        model->setBoardForTest(HPSDRHW::Saturn);
        model->configureStreamPool(5, 5, 192000);
        model->setConnectionStateForTest(ConnectionState::Connected);
        const int aId = model->addSlice(QStringLiteral("pan-0"));
        const int bId = model->addSlice(QStringLiteral("pan-0"));
        DesktopStationController controller(model, optionsFor(settings, directory.path()));
        window.setDesktopStationController(&controller);
        QVERIFY(controller.start(true));
        StationServer* server = controller.server();
        QVERIFY(server);
        TransmitHolder* holder = server->transmitHolder();
        QObject phoneSession;
        const DeviceSessionRegistry::Entry phone =
            admitPhone(*server, phoneSession, QByteArrayLiteral("phone-device-id-for-tx-badge-01"));
        QVERIFY(!phone.deviceId.isEmpty());
        TransmitHolder::KeyRequest key;
        key.deviceId = phone.deviceId;
        QCOMPARE(holder->askKey(key).verdict, KeyingVerdict::Admit);
        QVERIFY(holder->isHeldBy(phone.deviceId));
        TxSliceArbiter* arbiter = model->txSliceArbiter();
        QVERIFY(arbiter);
        const int boundBefore = arbiter->txBoundSliceId();
        const int target = boundBefore == bId ? aId : bId;
        const int other = target == aId ? bId : aId;
        VfoWidget* flag = flagFor(window, target);
        QVERIFY(flag);
        auto* badge = flag->findChild<QPushButton*>(QStringLiteral("VfoTxBadge"));
        QVERIFY(badge);
        QTRY_VERIFY(badge->isEnabled());
        QCOMPARE(badge->toolTip(),
                 QStringLiteral("Take transmit from iPhone and make this the TX slice"));
        QVERIFY(!flag->txSliceShown());

        // Refused (transmit is changing hands): nothing changes, and the
        // refusal is shown.
        holder->runWithKeyingBlocked([badge] { badge->click(); });
        QVERIFY(window.findChild<TakeTransmitDialog*>() == nullptr);
        QCOMPARE(toastsSaying(window, TxRefusals::changingHands().text), 1);
        QVERIFY(holder->isHeldBy(phone.deviceId));
        QCOMPARE(arbiter->txBoundSliceId(), boundBefore);

        // Cancelled: nothing changes.
        badge->click();
        TakeTransmitDialog* question = window.findChild<TakeTransmitDialog*>();
        QVERIFY(question);
        QCOMPARE(question->questionLabel()->text(), QStringLiteral("Take transmit from iPhone?"));
        QPointer<TakeTransmitDialog> cancelled(question);
        question->cancelButton()->click();
        QTRY_VERIFY(cancelled.isNull());
        QVERIFY(holder->isHeldBy(phone.deviceId));
        QCOMPARE(arbiter->txBoundSliceId(), boundBefore);
        QVERIFY(badge->isEnabled());

        // Taken: transmit is here and this slice is the TX slice.
        badge->click();
        question = window.findChild<TakeTransmitDialog*>();
        QVERIFY(question);
        QPointer<TakeTransmitDialog> taken(question);
        question->takeButton()->click();
        QTRY_VERIFY(taken.isNull());
        QTRY_VERIFY(holder->isHeldBy(SliceOwnership::stationDevice()));
        QTRY_COMPARE(arbiter->txBoundSliceId(), target);
        QTRY_VERIFY(flag->txSliceShown());
        QVERIFY(!model->mox());
        QVERIFY(!model->isTune());
        QVERIFY(badge->isEnabled());
        QCOMPARE(badge->toolTip(), QStringLiteral("Indicates this slice is the TX slice"));

        // Nobody holds transmit: the other slice's badge takes it at once.
        holder->release(SliceOwnership::stationDevice(), QStringLiteral("test hand-back"));
        QTRY_VERIFY(holder->state() == TransmitHolder::State::Unheld);
        VfoWidget* otherFlag = flagFor(window, other);
        QVERIFY(otherFlag);
        auto* otherBadge = otherFlag->findChild<QPushButton*>(QStringLiteral("VfoTxBadge"));
        QVERIFY(otherBadge);
        QTRY_VERIFY(otherBadge->isEnabled());
        QCOMPARE(otherBadge->toolTip(), QStringLiteral("Take transmit and make this the TX slice"));
        otherBadge->click();
        QVERIFY(window.findChild<TakeTransmitDialog*>() == nullptr);
        QTRY_VERIFY(holder->isHeldBy(SliceOwnership::stationDevice()));
        QTRY_COMPARE(arbiter->txBoundSliceId(), other);
        QTRY_VERIFY(otherFlag->txSliceShown());
        QVERIFY(!flag->txSliceShown());
        QVERIFY(!model->mox());
        QVERIFY(!model->isTune());

        // Case 1 as before: holding transmit, a click moves it.
        badge->click();
        QTRY_COMPARE(arbiter->txBoundSliceId(), target);
        QVERIFY(!model->mox());
        controller.stop();
    }

    // TX badge take, case 3 on the hosting desktop: the badge of a slice
    // the phone controls (this window listens) takes the slice, then asks
    // for transmit, and ends with this flag the TX flag. While the slice is
    // on the air it is held with the Core's words, and a take the Core
    // refuses there changes nothing. The phone has another slice, so it
    // keeps transmit when it loses this one (ruling Q8) and the transmit
    // question is asked; cancelled, the slice stays taken and transmit
    // stays with the phone.
    void hostTxBadgeOnAListenedSliceTakesTheSliceThenTransmit()
    {
        if (!QSslSocket::supportsSsl()) { QSKIP("Qt reports no working TLS backend."); }
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("station.settings")));
        MainWindow window({}, nullptr, MainWindow::ConnectionStartup::Deferred);
        RadioModel* model = window.radioModel();
        model->setBoardForTest(HPSDRHW::Saturn);
        model->configureStreamPool(5, 5, 192000);
        model->setConnectionStateForTest(ConnectionState::Connected);
        const int aId = model->addSlice(QStringLiteral("pan-0"));
        const int bId = model->addSlice(QStringLiteral("pan-0"));
        const int cId = model->addSlice(QStringLiteral("pan-0"));
        QVERIFY(aId >= 0 && cId >= 0);
        DesktopStationController controller(model, optionsFor(settings, directory.path()));
        window.setDesktopStationController(&controller);
        QVERIFY(controller.start(true));
        StationServer* server = controller.server();
        QVERIFY(server);
        TransmitHolder* holder = server->transmitHolder();
        QObject phoneSession;
        const DeviceSessionRegistry::Entry phone =
            admitPhone(*server, phoneSession, QByteArrayLiteral("phone-device-id-for-tx-badge-02"));
        QVERIFY(!phone.deviceId.isEmpty());
        SliceOwnership* ownership = model->sliceOwnership();
        const QByteArray station = SliceOwnership::stationDevice();
        ownership->setOwner(bId, phone.deviceId);
        ownership->setOwner(cId, phone.deviceId);
        VfoWidget* flag = flagFor(window, bId);
        QVERIFY(flag);
        QVERIFY(flag->isListening());
        auto* badge = flag->findChild<QPushButton*>(QStringLiteral("VfoTxBadge"));
        QVERIFY(badge);
        TransmitHolder::KeyRequest key;
        key.deviceId = phone.deviceId;
        QCOMPARE(holder->askKey(key).verdict, KeyingVerdict::Admit);
        QTRY_VERIFY(badge->isEnabled());
        QCOMPARE(badge->toolTip(),
                 QStringLiteral("Take control of this slice, then take transmit from iPhone"));

        // The phone on the air on B: held with the Core's on-air words.
        TxSliceArbiter* arbiter = model->txSliceArbiter();
        QVERIFY(arbiter);
        holder->setKeyed(true);
        if (arbiter->txBoundSliceId() != bId) {
            QVERIFY(arbiter->requestHandoff(bId, phone.deviceId));
        }
        QVERIFY(server->sliceOnAir(bId));
        const QString onAirWords =
            QStringLiteral("Slice %1 is transmitting. Take control once it stops.")
                .arg(QChar(QLatin1Char('A').unicode() + bId));
        QTRY_VERIFY(!badge->isEnabled());
        QCOMPARE(badge->toolTip(), onAirWords);
        QVERIFY(flag->txSliceShown());

        // A click the Core refuses on the air (an offer a moment old):
        // nothing changes, and the refusal is shown in its words.
        VfoWidget::TxBadgeOffer stale;
        stale.offered = true;
        stale.toolTip = QStringLiteral("Take control of this slice, then take transmit from iPhone");
        flag->setTxBadgeOffer(stale);
        flag->simulateTxBadgeClick();
        QCOMPARE(ownership->mark(bId).owner, phone.deviceId);
        QVERIFY(holder->isHeldBy(phone.deviceId));
        QVERIFY(window.findChild<TakeTransmitDialog*>() == nullptr);
        QTRY_COMPARE(toastsSaying(window, onAirWords), 1);
        holder->setKeyed(false);
        QVERIFY(!server->sliceOnAir(bId));
        QTRY_VERIFY(badge->isEnabled());
        QCOMPARE(badge->toolTip(),
                 QStringLiteral("Take control of this slice, then take transmit from iPhone"));

        // The slice is taken at once; the transmit question follows.
        // Cancelled: this window controls B, the phone keeps transmit.
        badge->click();
        QTRY_COMPARE(ownership->mark(bId).owner, station);
        // The phone's transmit moved to its other slice (ruling Q8).
        QVERIFY(holder->isHeldBy(phone.deviceId));
        QCOMPARE(arbiter->txBoundSliceId(), cId);
        TakeTransmitDialog* question = window.findChild<TakeTransmitDialog*>();
        QVERIFY(question);
        QCOMPARE(question->questionLabel()->text(), QStringLiteral("Take transmit from iPhone?"));
        QPointer<TakeTransmitDialog> cancelled(question);
        question->cancelButton()->click();
        QTRY_VERIFY(cancelled.isNull());
        QVERIFY(holder->isHeldBy(phone.deviceId));
        QTRY_VERIFY(!flag->isListening());
        QVERIFY(!flag->txSliceShown());
        // Now case 2 for this flag.
        QTRY_COMPARE(badge->toolTip(),
                     QStringLiteral("Take transmit from iPhone and make this the TX slice"));
        QVERIFY(badge->isEnabled());

        // Give the slice back to the phone and take both in one tap.
        ownership->setOwner(bId, phone.deviceId);
        QTRY_VERIFY(flag->isListening());
        QTRY_VERIFY(badge->isEnabled());
        badge->click();
        QTRY_COMPARE(ownership->mark(bId).owner, station);
        question = window.findChild<TakeTransmitDialog*>();
        QVERIFY(question);
        QPointer<TakeTransmitDialog> taken(question);
        question->takeButton()->click();
        QTRY_VERIFY(taken.isNull());
        QTRY_VERIFY(holder->isHeldBy(station));
        QTRY_COMPARE(arbiter->txBoundSliceId(), bId);
        QTRY_VERIFY(flag->txSliceShown());
        QVERIFY(!model->mox());
        QVERIFY(!model->isTune());
        controller.stop();
    }

    // TX badge take fix round 1: only the badge's own take makes its slice
    // the TX slice. A keyed holder is unkeyed and taken over through the
    // badge. A take the Core does not assign (MOX read on past the wait,
    // stopNotConfirmed) and a take cut off by hosting stopping leave
    // nothing pending, so a later grant of transmit selects no slice.
    // Nothing keys: MOX is read as on through the holder's hook only.
    void hostTxBadgeTakeEndsWithNothingPending()
    {
        if (!QSslSocket::supportsSsl()) { QSKIP("Qt reports no working TLS backend."); }
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("station.settings")));
        MainWindow window({}, nullptr, MainWindow::ConnectionStartup::Deferred);
        RadioModel* model = window.radioModel();
        model->setBoardForTest(HPSDRHW::Saturn);
        model->configureStreamPool(5, 5, 192000);
        model->setConnectionStateForTest(ConnectionState::Connected);
        const int aId = model->addSlice(QStringLiteral("pan-0"));
        const int bId = model->addSlice(QStringLiteral("pan-0"));
        QVERIFY(aId >= 0 && bId >= 0);
        DesktopStationController controller(model, optionsFor(settings, directory.path()));
        window.setDesktopStationController(&controller);
        QVERIFY(controller.start(true));
        StationServer* server = controller.server();
        QVERIFY(server);
        TransmitHolder* holder = server->transmitHolder();
        QObject phoneSession;
        const DeviceSessionRegistry::Entry phone =
            admitPhone(*server, phoneSession, QByteArrayLiteral("phone-device-id-for-tx-badge-04"));
        QVERIFY(!phone.deviceId.isEmpty());
        TransmitHolder::KeyRequest key;
        key.deviceId = phone.deviceId;
        QCOMPARE(holder->askKey(key).verdict, KeyingVerdict::Admit);
        TxSliceArbiter* arbiter = model->txSliceArbiter();
        QVERIFY(arbiter);
        VfoWidget* flagA = flagFor(window, aId);
        VfoWidget* flagB = flagFor(window, bId);
        QVERIFY(flagA && flagB);
        auto* badgeA = flagA->findChild<QPushButton*>(QStringLiteral("VfoTxBadge"));
        auto* badgeB = flagB->findChild<QPushButton*>(QStringLiteral("VfoTxBadge"));
        QVERIFY(badgeA && badgeB);
        const QByteArray station = SliceOwnership::stationDevice();

        // The phone on the air: the badge's question unkeys it and takes
        // over, and B is the TX slice.
        holder->setKeyed(true);
        QTRY_VERIFY(badgeB->isEnabled());
        badgeB->click();
        TakeTransmitDialog* question = window.findChild<TakeTransmitDialog*>();
        QVERIFY(question);
        QCOMPARE(question->takeButton()->text(), QStringLiteral("Unkey and take over"));
        QPointer<TakeTransmitDialog> unkeyed(question);
        question->takeButton()->click();
        QTRY_VERIFY(unkeyed.isNull());
        QTRY_VERIFY(holder->isHeldBy(station));
        QTRY_COMPARE(arbiter->txBoundSliceId(), bId);
        QTRY_VERIFY(flagB->txSliceShown());
        QCOMPARE(controller.takeInFlight(), quint64(0));
        QVERIFY(!model->mox());
        QVERIFY(!model->isTune());

        // Back to the phone.
        const auto phoneHolds = [&]() {
            holder->release(station, QStringLiteral("test hand-back"));
            return QTest::qWaitFor([holder] {
                       return holder->state() == TransmitHolder::State::Unheld;
                   })
                && holder->askKey(key).verdict == KeyingVerdict::Admit
                && holder->isHeldBy(phone.deviceId);
        };
        QVERIFY(phoneHolds());

        // Not assigned: MOX reads on past the wait, so the Core does not
        // confirm the stop and the take ends with transmit unheld.
        const TransmitHolder::Hooks saved = holder->hooks();
        bool moxReadsOn = true;
        TransmitHolder::Hooks forced = saved;
        forced.moxOn = [&moxReadsOn]() { return moxReadsOn; };
        holder->setHooks(forced);
        QTRY_VERIFY(badgeA->isEnabled());
        badgeA->click();
        question = window.findChild<TakeTransmitDialog*>();
        QVERIFY(question);
        QPointer<TakeTransmitDialog> notConfirmed(question);
        question->takeButton()->click();
        QTRY_VERIFY(notConfirmed.isNull());
        QTRY_VERIFY_WITH_TIMEOUT(controller.takeInFlight() == 0,
                                 TransmitHolder::kMoxOffWaitMs + 3000);
        QVERIFY(!holder->isHeldBy(station));
        // While the Core refuses a take for now, the badge is held with
        // its words.
        QTRY_VERIFY(!badgeA->isEnabled());
        QCOMPARE(badgeA->toolTip(), TxRefusals::stopNotConfirmed().text);
        moxReadsOn = false;
        holder->setHooks(saved);
        holder->onMoxReading(false);
        QTRY_VERIFY(badgeA->isEnabled());
        QCOMPARE(badgeA->toolTip(), QStringLiteral("Take transmit and make this the TX slice"));
        // A later grant of transmit to this window selects no slice.
        QSignalSpy selected(model, &RadioModel::txSliceSelected);
        QCOMPARE(controller.requestTakeTransmit().state,
                 DesktopStationController::RequestState::Pending);
        QTRY_VERIFY(holder->isHeldBy(station));
        QTest::qWait(300);
        QVERIFY(selected.isEmpty());
        QVERIFY(!model->mox());

        // Hosting stops with the question open: nothing is left for a
        // later host's grant.
        QVERIFY(phoneHolds());
        QTRY_VERIFY(badgeA->isEnabled());
        badgeA->click();
        question = window.findChild<TakeTransmitDialog*>();
        QVERIFY(question);
        QPointer<TakeTransmitDialog> open(question);
        controller.stop();
        QTRY_VERIFY(open.isNull() || !open->isVisible());
        QTemporaryDir directory2;
        QVERIFY(directory2.isValid());
        AppSettings settings2(directory2.filePath(QStringLiteral("station.settings")));
        DesktopStationController controller2(model, optionsFor(settings2, directory2.path()));
        window.setDesktopStationController(&controller2);
        QVERIFY(controller2.start(true));
        selected.clear();
        QCOMPARE(controller2.requestTakeTransmit().state,
                 DesktopStationController::RequestState::Pending);
        QTRY_VERIFY(controller2.server()->transmitHolder()->isHeldBy(station));
        QTest::qWait(300);
        QVERIFY(selected.isEmpty());
        QVERIFY(!model->mox());
        controller2.stop();
    }

    // TX badge take, case 3 with nothing more to ask: with transmit
    // already here, the slice take alone; and when the phone holds transmit
    // on the one slice it has, the take of that slice frees transmit
    // (ruling Q8), so it is taken at once. Either way one tap ends with
    // this flag the TX flag and no transmit question.
    void hostTxBadgeOnAListenedSliceAsksOnlyWhatItMust()
    {
        if (!QSslSocket::supportsSsl()) { QSKIP("Qt reports no working TLS backend."); }
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("station.settings")));
        MainWindow window({}, nullptr, MainWindow::ConnectionStartup::Deferred);
        RadioModel* model = window.radioModel();
        model->setBoardForTest(HPSDRHW::Saturn);
        model->configureStreamPool(5, 5, 192000);
        model->setConnectionStateForTest(ConnectionState::Connected);
        QVERIFY(model->addSlice(QStringLiteral("pan-0")) >= 0);
        const int bId = model->addSlice(QStringLiteral("pan-0"));
        DesktopStationController controller(model, optionsFor(settings, directory.path()));
        window.setDesktopStationController(&controller);
        QVERIFY(controller.start(true));
        StationServer* server = controller.server();
        QVERIFY(server);
        TransmitHolder* holder = server->transmitHolder();
        QCOMPARE(controller.requestTakeTransmit().state,
                 DesktopStationController::RequestState::Pending);
        QTRY_VERIFY(holder->isHeldBy(SliceOwnership::stationDevice()));
        QVERIFY(!model->mox());
        QObject phoneSession;
        const DeviceSessionRegistry::Entry phone =
            admitPhone(*server, phoneSession, QByteArrayLiteral("phone-device-id-for-tx-badge-03"));
        QVERIFY(!phone.deviceId.isEmpty());
        SliceOwnership* ownership = model->sliceOwnership();
        ownership->setOwner(bId, phone.deviceId);
        VfoWidget* flag = flagFor(window, bId);
        QVERIFY(flag && flag->isListening());
        auto* badge = flag->findChild<QPushButton*>(QStringLiteral("VfoTxBadge"));
        QVERIFY(badge);
        QTRY_VERIFY(badge->isEnabled());
        QCOMPARE(badge->toolTip(),
                 QStringLiteral("Take control of this slice and make it the TX slice"));
        badge->click();
        QTRY_COMPARE(ownership->mark(bId).owner, SliceOwnership::stationDevice());
        QVERIFY(window.findChild<TakeTransmitDialog*>() == nullptr);
        QTRY_COMPARE(model->txSliceArbiter()->txBoundSliceId(), bId);
        QTRY_VERIFY(flag->txSliceShown());
        QVERIFY(!model->mox());
        QVERIFY(!model->isTune());

        // The phone holds transmit on B, its only slice.
        holder->release(SliceOwnership::stationDevice(), QStringLiteral("test hand-back"));
        QTRY_VERIFY(holder->state() == TransmitHolder::State::Unheld);
        ownership->setOwner(bId, phone.deviceId);
        QTRY_VERIFY(flag->isListening());
        TransmitHolder::KeyRequest key;
        key.deviceId = phone.deviceId;
        QCOMPARE(holder->askKey(key).verdict, KeyingVerdict::Admit);
        TxSliceArbiter* arbiter = model->txSliceArbiter();
        if (arbiter->txBoundSliceId() != bId) {
            QVERIFY(arbiter->requestHandoff(bId, phone.deviceId));
        }
        QTRY_VERIFY(badge->isEnabled());
        QCOMPARE(badge->toolTip(),
                 QStringLiteral("Take control of this slice, then take transmit from iPhone"));
        badge->click();
        QTRY_COMPARE(ownership->mark(bId).owner, SliceOwnership::stationDevice());
        QVERIFY(window.findChild<TakeTransmitDialog*>() == nullptr);
        QTRY_VERIFY(holder->isHeldBy(SliceOwnership::stationDevice()));
        QTRY_COMPARE(arbiter->txBoundSliceId(), bId);
        QTRY_VERIFY(flag->txSliceShown());
        QVERIFY(!model->mox());
        QVERIFY(!model->isTune());
        controller.stop();
    }
};

QTEST_MAIN(TstDesktopStationWindow)
#include "tst_desktop_station_window.moc"
