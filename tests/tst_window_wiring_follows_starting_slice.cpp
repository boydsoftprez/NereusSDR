// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_window_wiring_follows_starting_slice.cpp  (NereusSDR)
// =================================================================
//
// JJ 2026-10-23: "the core headless and core inside window should be the
// same experience." A Core inside the desktop window keeps Slice A for a
// paired device that had it on a headless Core, so the window starts on B.
// The window's one-time wiring (the spectrum's first view, the restore
// push, the mode-driven applets, the mode menu) must follow the
// window's own slice B, not Slice A: when the device signs in on A, its
// frequency and mode must not move the window's spectrum or applets.
//
// The model runs the connect's starting-slice step (addStartingSlices) and
// its state restore (loadSliceState) in a real MainWindow; the device's
// slice is placed by the sign-in step (placeReservedSlicesFor).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-23: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <QAction>

#include "core/AppSettings.h"
#include "core/ReceiveLayoutStore.h"
#include "core/SliceOwnership.h"
#include "fakes/MainWindowTestSettings.h"
#include "gui/MainWindow.h"
#include "gui/PanadapterStack.h"
#include "gui/SpectrumWidget.h"
#include "gui/applets/AppletVisibilityController.h"
#include "gui/widgets/VfoWidget.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

const QString kMac = QStringLiteral("aa:bb:cc:11:22:33");
const QByteArray kPhone = QByteArray(32, '\x5a');
constexpr double kWindowFrequencyHz = 3865000.0;
constexpr double kPhoneFrequencyHz = 7074000.0;

RadioInfo benchRadio()
{
    RadioInfo info;
    info.macAddress = kMac;
    info.name = QStringLiteral("Bench HL2");
    info.boardType = HPSDRHW::HermesLite;
    return info;
}

// What the window saved for its own Slice B on an earlier run.
void saveWindowSliceB()
{
    RadioModel seed;
    seed.setBoardForTest(HPSDRHW::HermesLite);
    seed.addSlice(QStringLiteral("pan-0"));
    const int b = seed.addSlice(QStringLiteral("pan-0"));
    SliceModel* slice = seed.sliceById(b);
    QVERIFY(slice);
    QCOMPARE(b, 1);
    slice->setSettingsRadioIdentity(kMac);
    slice->setFrequency(kWindowFrequencyHz);
    slice->setDspMode(DSPMode::LSB);
    slice->saveToSettings(slice->band());
}

// The headless Core's layout: the paired phone on Slice A.
void savePhoneOnSliceA()
{
    ReceiveSliceState a;
    a.id = 0;
    a.panKey = QStringLiteral("pan-0");
    a.frequencyHz = kPhoneFrequencyHz;
    a.dspMode = DSPMode::USB;
    a.owner = kPhone;
    QString reason;
    QVERIFY2(ReceiveLayoutStore::stage(AppSettings::instance(), kMac, {a}, &reason,
                                       std::nullopt, std::nullopt),
             qPrintable(reason));
}

// The RADE applet is offered while the window's slice is in a RADE mode.
// It is set by the same mode wiring as the Phone/CW page, whose CW and FM
// pages are not built yet (R-R3-49), so it is the one that can be seen.
bool radeOffered(MainWindow& window)
{
    auto* visibility = window.findChild<AppletVisibilityController*>();
    return visibility && visibility->isAvailable(QStringLiteral("Rade"));
}

QAction* modeAction(MainWindow& window, const QString& text)
{
    for (QAction* action : window.findChildren<QAction*>()) {
        if (action->isCheckable() && action->text() == text) {
            return action;
        }
    }
    return nullptr;
}

} // namespace

class TestWindowWiringFollowsStartingSlice : public QObject {
    Q_OBJECT

private slots:
    void init()
    {
        AppSettings::instance().clear();
        Test::markAudioFirstRunDone();
    }
    void cleanup() { AppSettings::instance().clear(); }

    void theWindowsWiringFollowsItsOwnSliceWhenADeviceKeepsA()
    {
        saveWindowSliceB();
        savePhoneOnSliceA();

        MainWindow window({}, nullptr, MainWindow::ConnectionStartup::Deferred);
        window.resize(1280, 800);
        window.show();
        RadioModel* radio = window.radioModel();
        radio->setBoardForTest(HPSDRHW::HermesLite);
        radio->setLastRadioInfoForTest(benchRadio());
        radio->setHostsStationDevice(true);

        // The connect: the starting slice, then its saved state.
        const int windowSlice = radio->addStartingSlices(kMac);
        QCOMPARE(windowSlice, 1);
        QCOMPARE(radio->windowSliceId(), 1);
        QCOMPARE(radio->reservedSliceLetters(), QList<int>{0});
        SliceModel* b = radio->sliceById(windowSlice);
        QVERIFY(b);
        radio->setActiveSlice(static_cast<int>(radio->slices().indexOf(b)));
        radio->loadSliceState(b);
        QCOMPARE(b->frequency(), kWindowFrequencyHz);
        radio->setConnectionStateForTest(ConnectionState::Connected);

        PanadapterStack* stack = window.findChild<PanadapterStack*>();
        QVERIFY(stack);
        SpectrumWidget* spectrum = stack->spectrum(QStringLiteral("pan-0"));
        QVERIFY(spectrum);
        QTRY_VERIFY(spectrum->vfoWidget(windowSlice));

        // The restore push reached the spectrum with the window's frequency.
        QCOMPARE(spectrum->ddcCenterFrequency(), kWindowFrequencyHz);
        QCOMPARE(spectrum->vfoFrequencyForTest(), kWindowFrequencyHz);

        // The applets and the mode menu follow the window's slice.
        b->setDspMode(DSPMode::USB);
        QVERIFY(!radeOffered(window));
        b->setDspMode(DSPMode::RADE_U);
        QVERIFY(radeOffered(window));
        b->setDspMode(DSPMode::CWU);
        QVERIFY(!radeOffered(window));
        QAction* cwu = modeAction(window, QStringLiteral("CWU"));
        QVERIFY(cwu);
        QVERIFY(cwu->isChecked());

        // The phone signs in and gets A, at its own frequency and mode.
        QCOMPARE(radio->placeReservedSlicesFor(kPhone), 1);
        SliceModel* a = radio->sliceById(0);
        QVERIFY(a);
        QCOMPARE(radio->sliceOwnership()->mark(0).owner, kPhone);
        QCOMPARE(a->frequency(), kPhoneFrequencyHz);
        b->setDspMode(DSPMode::RADE_U);
        QVERIFY(radeOffered(window));
        a->setDspMode(DSPMode::FM);
        QCoreApplication::processEvents();

        // None of it moved the window's spectrum or applets. The pan's own
        // VFO marker is not checked here: making any slice's flag on a pan
        // sets it (createSliceFlag), the same for a device's slice on B
        // beside a window on A, so it is not the window's one-time wiring.
        QCOMPARE(spectrum->ddcCenterFrequency(), kWindowFrequencyHz);
        QVERIFY(radeOffered(window));
        QAction* radeU = modeAction(window, QStringLiteral("RADE-U"));
        QVERIFY(radeU);
        QVERIFY(radeU->isChecked());
        QCOMPARE(radio->activeSlice(), b);
    }

    // With no layout the window starts on A and is wired exactly as before.
    void theWindowStartsOnAWithNoLayout()
    {
        saveWindowSliceB();
        MainWindow window({}, nullptr, MainWindow::ConnectionStartup::Deferred);
        window.resize(1280, 800);
        window.show();
        RadioModel* radio = window.radioModel();
        radio->setBoardForTest(HPSDRHW::HermesLite);
        radio->setLastRadioInfoForTest(benchRadio());
        radio->setHostsStationDevice(true);

        const int windowSlice = radio->addStartingSlices(kMac);
        QCOMPARE(windowSlice, 0);
        QCOMPARE(radio->windowSliceId(), 0);
        SliceModel* a = radio->sliceById(0);
        QVERIFY(a);
        radio->setActiveSlice(0);
        radio->loadSliceState(a);
        radio->setConnectionStateForTest(ConnectionState::Connected);

        PanadapterStack* stack = window.findChild<PanadapterStack*>();
        QVERIFY(stack);
        SpectrumWidget* spectrum = stack->spectrum(QStringLiteral("pan-0"));
        QVERIFY(spectrum);
        QTRY_VERIFY(spectrum->vfoWidget(0));
        QCOMPARE(spectrum->ddcCenterFrequency(), a->frequency());
        a->setDspMode(DSPMode::RADE_U);
        QVERIFY(radeOffered(window));
        a->setDspMode(DSPMode::USB);
        QVERIFY(!radeOffered(window));
    }
};

QTEST_MAIN(TestWindowWiringFollowsStartingSlice)
#include "tst_window_wiring_follows_starting_slice.moc"
