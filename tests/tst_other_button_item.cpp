// =================================================================
// tests/tst_other_button_item.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original test. It drives the container function
// and band buttons through ContainerButtonDispatcher against a real
// RadioModel; no upstream logic is ported here.
//
// R3 unfinished controls, Task 3 (R-R3-49, R-R3-21): the container function
// buttons whose feature NereusSDR has work, each on its container's own
// slice; the rest are hidden through UnbuiltFeatures.
//   - The fifteen connected buttons are drawn; the other nineteen, the
//     macro buttons, filter Var1 / Var2, antenna XVTR and Rx/Tx and band
//     XVTR are not, and the saved visibility is unchanged.
//   - A container set to slice A acts on slice A while slice B is active
//     (ANF, SNB, Mute, BIN, the band buttons), and lights from slice A.
//   - The band buttons light the container slice's band and follow it.
//   - A container set to a slice that is not open shows its buttons
//     unavailable with a plain reason, and a click changes nothing.
//   - With no radio, TUN, MOX and 2TON are unavailable and do nothing;
//     MON, MNF, Peak, CTUN and VAX act on their targets.
//   - There is no Power button (maintainer decision 2026-09-30): it is not
//     drawn, a saved one is dropped on load, and an old Power id clicked
//     changes nothing.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-24  J.J. Boyd / KG4VCF  R3 unfinished controls, Task 3.
//                                    AI-assisted transformation via
//                                    Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  R3 unfinished controls, fix wave.
//                                    AI-assisted transformation via
//                                    Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  Receiver and transmit gaps plan,
//                                    Task 7 fix wave: the container MOX
//                                    button is blocked by TX inhibit and
//                                    the PA trip, and is a manual key. AI-assisted
//                                    transformation via Anthropic Claude
//                                    Code.
//   2026-09-27  J.J. Boyd / KG4VCF  Parity Task 31: DUP is built (display
//                                    duplex) and acts on the window's
//                                    setting through the dispatcher's
//                                    hooks. AI-assisted via Anthropic
//                                    Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  Slice control plan Task 15 fix round
//                                    1: a slice another device controls
//                                    refuses the slice buttons with the
//                                    window's reason. AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-30  J.J. Boyd / KG4VCF  The Power button is removed
//                                    (maintainer decision): not drawn,
//                                    dropped from a saved layout, and an
//                                    old id does nothing. AI-assisted via
//                                    Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QMouseEvent>
#include <QSignalSpy>
#include <QStandardPaths>

#include "OperatorWording.h"
#include "fakes/FakeAudioBus.h"

#include "core/AppSettings.h"
#include "core/AudioEngine.h"
#include "core/ConnectionState.h"
#include "core/MoxController.h"
#include "core/TwoToneController.h"
#include "gui/SpectrumWidget.h"
#include "gui/UnbuiltFeatures.h"
#include "gui/containers/ContainerButtonDispatcher.h"
#include "gui/containers/ContainerContentRegistry.h"
#include "gui/containers/ContainerWidget.h"
#include "gui/meters/AntennaButtonItem.h"
#include "gui/meters/BandButtonItem.h"
#include "gui/meters/FilterButtonItem.h"
#include "gui/meters/ModeButtonItem.h"
#include "gui/meters/OtherButtonItem.h"
#include "models/Band.h"
#include "models/NotchModel.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

#include <memory>

using namespace NereusSDR;
using Id = OtherButtonItem::ButtonId;

namespace {

constexpr int kSliceA = 1;  // ContainerWidget::rxSource() for slice A
constexpr int kSliceB = 2;
constexpr int kSliceC = 3;

const QList<Id> kConnected = {
    Id::Mon, Id::Tun, Id::Mox, Id::TwoTon, Id::PsA,
    Id::Anf, Id::Snb, Id::Mnf, Id::PeakHold, Id::Ctun,
    Id::Vac1, Id::Vac2, Id::Mute, Id::Bin, Id::Dup,
};

const QList<Id> kHidden = {
    Id::Rx2, Id::SubRx, Id::PanSwap, Id::Play, Id::Rec, Id::Xpa, Id::Avg,
    Id::Spectrum, Id::Panadapter, Id::Scope, Id::Scope2, Id::Phase, Id::Waterfall,
    Id::Histogram, Id::Panafall, Id::Panascope, Id::Spectrascope, Id::DisplayOff,
};

void clearBandKeys()
{
    auto& s = AppSettings::instance();
    for (const QString& key : s.allKeys()) {
        if (key.startsWith(QStringLiteral("Slice")) && key.contains(QStringLiteral("/Band"))) {
            s.remove(key);
        }
    }
}

// Press and release the mouse over the shown button at `index` on an item
// laid over a 600 x 600 widget (six columns, as OtherButtonItem sets).
void clickShownButton(ButtonBoxItem& item, int index)
{
    int visible = 0;
    for (int i = 0; i < index; ++i) {
        if (item.isButtonShown(i)) { ++visible; }
    }
    const double cell = 600.0 * 0.96 / item.columns();
    const QPointF at(12.0 + (visible % item.columns()) * cell + cell / 3.0,
                     12.0 + (visible / item.columns()) * cell + cell / 3.0);
    QMouseEvent press(QEvent::MouseButtonPress, at, at, Qt::LeftButton,
                      Qt::LeftButton, Qt::NoModifier);
    QMouseEvent release(QEvent::MouseButtonRelease, at, at, Qt::LeftButton,
                        Qt::NoButton, Qt::NoModifier);
    item.handleMousePress(&press, 600, 600);
    item.handleMouseRelease(&release, 600, 600);
}

} // namespace

class TstOtherButtonItem : public QObject {
    Q_OBJECT

private:
    // A local model with slices A and B, B active, and no radio.
    struct Fixture {
        RadioModel model;
        SliceModel* a{nullptr};
        SliceModel* b{nullptr};
        SpectrumWidget spectrumA;
        SpectrumWidget spectrumB;
        std::unique_ptr<AudioEngine> vax;
        std::unique_ptr<ContainerButtonDispatcher> dispatcher;

        Fixture()
        {
            model.addSlice();
            model.addSlice();
            a = model.sliceById(0);
            b = model.sliceById(1);
            model.setActiveSliceById(1);
            a->setFrequency(14100000.0);
            b->setFrequency(3700000.0);

            vax = std::make_unique<AudioEngine>();
            vax->setVaxBusFactoryForTest([](int channel) -> std::unique_ptr<IAudioBus> {
                auto bus = std::make_unique<FakeAudioBus>(QStringLiteral("FakeVax%1").arg(channel));
                AudioFormat fmt;
                fmt.sampleRate = 48000;
                fmt.channels = 2;
                fmt.sample = AudioFormat::Sample::Float32;
                bus->open(fmt);
                return bus;
            });

            ContainerButtonDispatcher::Hooks hooks;
            hooks.transmitPermitted = [] { return true; };
            hooks.spectrumFor = [this](SliceModel* s) -> SpectrumWidget* {
                return s == a ? &spectrumA : (s == b ? &spectrumB : nullptr);
            };
            hooks.vaxDevices = vax.get();
            dispatcher = std::make_unique<ContainerButtonDispatcher>(&model, std::move(hooks));
        }
    };

private slots:
    void independentTransmitControlsUseExactlyOneGuardedExistingHook()
    {
        Fixture f;f.model.setConnectionStateForTest(ConnectionState::Connected);
        ContainerContentRegistry registry;
        for(const auto& action:QList<QPair<QString,Id>>{{"mox",Id::Mox},{"tune",Id::Tun},{"twoTone",Id::TwoTon}}) {
            int requests=0;bool on=false;
            ContainerButtonDispatcher::Hooks hooks;hooks.desktopHosting=[]{return true;};hooks.desktopMoxOn=[&]{return on;};hooks.desktopTuneOn=[&]{return on;};
            const auto request=[&](bool value){++requests;on=value;};hooks.requestDesktopMox=request;hooks.requestDesktopTune=request;hooks.requestDesktopTwoTone=request;
            ContainerButtonDispatcher dispatcher(&f.model,std::move(hooks));const auto entry=registry.makeEntry("control."+action.first);
            for(const auto mode:{ContentRenderMode::Preview,ContentRenderMode::Live}) {
                std::unique_ptr<MeterItem> item(registry.createMeterItem(entry,nullptr,mode));auto* other=qobject_cast<OtherButtonItem*>(item.get());QVERIFY(other);other->setRect(0,0,1,1);
                connect(other,&OtherButtonItem::otherButtonClicked,this,[&](int id){QVERIFY(dispatcher.click(Id(id),kSliceA).isEmpty());});dispatcher.apply(other,kSliceA);
                QMouseEvent press(QEvent::MouseButtonPress,QPointF(55,20),QPointF(55,20),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);QMouseEvent release(QEvent::MouseButtonRelease,QPointF(55,20),QPointF(55,20),Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
                other->handleMousePress(&press,112,44);other->handleMouseRelease(&release,112,44);QCOMPARE(requests,mode==ContentRenderMode::Live?1:0);
                QVERIFY(!f.model.moxController()->isMox());QVERIFY(!f.model.isTune());QVERIFY(!f.model.twoToneController()->isActive());
                if(mode==ContentRenderMode::Live) {f.model.setRxOnly(true);dispatcher.apply(other,kSliceA);QVERIFY(!other->isButtonAvailable(action.second));QSignalSpy refused(other,&ButtonBoxItem::unavailableButtonClicked);other->handleMousePress(&press,112,44);other->handleMouseRelease(&release,112,44);QCOMPARE(requests,1);QCOMPARE(refused.count(),1);f.model.setRxOnly(false);}
            }
        }
        f.model.setConnectionStateForTest(ConnectionState::Disconnected);
    }
    void initTestCase()
    {
        // The engine opens no real sound device in test mode.
        QStandardPaths::setTestModeEnabled(true);
    }

    void init()
    {
        clearBandKeys();
        UnbuiltFeatures::resetForTest();
    }

    void cleanup()
    {
        clearBandKeys();
        AppSettings::instance().remove(QStringLiteral("audio/Vax1/Enabled"));
        AppSettings::instance().remove(QStringLiteral("audio/Vax2/Enabled"));
        UnbuiltFeatures::resetForTest();
    }

    // ── The button list ─────────────────────────────────────────────────

    void connectedButtonsAreDrawnAndTheRestAreNot()
    {
        OtherButtonItem item;
        for (Id id : kConnected) {
            QVERIFY2(item.isButtonShown(id),
                     qPrintable(QStringLiteral("button %1 is not drawn").arg(int(id))));
            QVERIFY(!OtherButtonItem::unbuiltFeatureFor(id).has_value());
        }
        for (Id id : kHidden) {
            QVERIFY2(!item.isButtonShown(id),
                     qPrintable(QStringLiteral("button %1 is drawn").arg(int(id))));
            QVERIFY(OtherButtonItem::unbuiltFeatureFor(id).has_value());
        }
        // Power is neither: NereusSDR has no Power button.
        QVERIFY(!item.isButtonShown(Id::Power));
        QCOMPARE(kConnected.size() + kHidden.size() + 1, 34);
        // The macro buttons stay hidden.
        for (int i = 34; i < item.buttonCount(); ++i) {
            QVERIFY(!item.ButtonBoxItem::isButtonShown(i));
        }
        // Hiding wrote nothing into the saved visibility; only Power's bit
        // is cleared.
        QCOMPARE(item.visibleBits(), 0xFFFFFFFEu);

        // VAC1 / VAC2 read VAX 1 / VAX 2.
        QCOMPARE(item.button(item.indexOf(Id::Vac1)).text, QStringLiteral("VAX 1"));
        QCOMPARE(item.button(item.indexOf(Id::Vac2)).text, QStringLiteral("VAX 2"));

        FilterButtonItem filters;
        QVERIFY(!filters.isButtonShown(10));  // Var1
        QVERIFY(!filters.isButtonShown(11));  // Var2
        QVERIFY(filters.isButtonShown(0));
        AntennaButtonItem antennas;
        QVERIFY(!antennas.isButtonShown(5));  // XVTR input
        QVERIFY(!antennas.isButtonShown(9));  // Rx/Tx
        QVERIFY(antennas.isButtonShown(3));
        BandButtonItem bands;
        QVERIFY(!bands.isButtonShown(uiIndexFromBand(Band::XVTR)));
        QVERIFY(bands.isButtonShown(uiIndexFromBand(Band::Band20m)));
    }

    void aHiddenButtonComesBackWhenItsFeatureIsBuilt()
    {
        UnbuiltFeatures::setBuiltForTest(UnbuiltFeature::Voice, true);
        OtherButtonItem item;
        QVERIFY(item.isButtonShown(Id::Play));
        QVERIFY(item.isButtonShown(Id::Rec));
        QVERIFY(!item.isButtonShown(Id::Xpa));
    }

    // Parity Task 31 (A11): DUP is built (display duplex); the status bar's
    // FDX (full duplex) is not, and building it shows no container button.
    void theDupButtonIsTheWindowsDisplayDuplex()
    {
        OtherButtonItem item;
        QVERIFY(item.isButtonShown(Id::Dup));
        QVERIFY(!OtherButtonItem::unbuiltFeatureFor(Id::Dup).has_value());
        QVERIFY(!UnbuiltFeatures::isBuilt(UnbuiltFeature::Fdx));

        Fixture f;
        bool duplex = false;
        QString reason;
        ContainerButtonDispatcher::Hooks hooks;
        hooks.displayDuplexOn = [&duplex] { return duplex; };
        hooks.setDisplayDuplex = [&duplex](bool on) { duplex = on; };
        hooks.displayDuplexReason = [&reason] { return reason; };
        ContainerButtonDispatcher dispatcher(&f.model, std::move(hooks));
        QVERIFY(dispatcher.click(Id::Dup, kSliceA).isEmpty());
        QVERIFY(duplex);
        dispatcher.apply(&item, kSliceA);
        QVERIFY(item.buttonState(Id::Dup));
        QVERIFY(item.isButtonAvailable(Id::Dup));
        QVERIFY(dispatcher.click(Id::Dup, kSliceA).isEmpty());
        QVERIFY(!duplex);
        // A Core below txDisplayVersion 3: unavailable with its reason, and
        // a click changes nothing.
        reason = QStringLiteral("This Core does not show the receiver while transmitting for "
                                "this app. Updating the Core may help.");
        dispatcher.apply(&item, kSliceA);
        QVERIFY(!item.isButtonAvailable(Id::Dup));
        QCOMPARE(dispatcher.click(Id::Dup, kSliceA), reason);
        QVERIFY(!duplex);
        QVERIFY(OperatorWording::isPlain(reason));
        // With no hooks (the default fixture) it says it does nothing.
        QVERIFY(!f.dispatcher->stateOf(Id::Dup, kSliceA).available);
    }

    // Fix wave I1: the macro buttons, AVG and the two-receiver layout
    // (RX2, SUB, SWAP) each have an entry of their own, so marking the
    // macro buttons built draws them and nothing else.
    void markingTheMacroButtonsBuiltShowsOnlyTheMacroButtons()
    {
        UnbuiltFeatures::setBuiltForTest(UnbuiltFeature::MacroButtons, true);
        OtherButtonItem item;
        for (int i = 34; i < item.buttonCount(); ++i) {  // after the 34 core buttons
            QVERIFY2(!item.button(i).hiddenUntilBuilt,
                     qPrintable(QStringLiteral("macro %1 still hidden").arg(i)));
        }
        for (Id id : {Id::Rx2, Id::SubRx, Id::PanSwap, Id::Avg}) {
            QVERIFY2(!item.isButtonShown(id),
                     qPrintable(QStringLiteral("button %1 is drawn").arg(int(id))));
        }
    }

    void savedVisibilityOfAHiddenButtonIsKept()
    {
        OtherButtonItem item;
        // Every bit but Power's: Power is not a button in NereusSDR and is
        // dropped on load (savedPowerButtonIsDroppedOnLoad covers that).
        const QString saved = QStringLiteral("OTHERBTNS|0|0|1|1|0|0|6|4294967294");
        QVERIFY(item.deserialize(saved));
        QVERIFY(!item.isButtonShown(Id::Rx2));
        QVERIFY(item.isButtonShown(Id::Anf));
        QCOMPARE(item.serialize(), saved);
    }

    // ── The container's own slice ───────────────────────────────────────

    void sliceButtonsActOnTheContainersSliceNotTheActiveOne()
    {
        Fixture f;
        QCOMPARE(f.model.activeSlice(), f.b);
        OtherButtonItem item;

        struct Case { Id id; std::function<bool(SliceModel*)> read; };
        const QList<Case> cases = {
            {Id::Anf, [](SliceModel* s) { return s->anfEnabled(); }},
            {Id::Snb, [](SliceModel* s) { return s->snbEnabled(); }},
            {Id::Mute, [](SliceModel* s) { return s->muted(); }},
            {Id::Bin, [](SliceModel* s) { return s->binauralEnabled(); }},
        };
        for (const Case& c : cases) {
            const bool aBefore = c.read(f.a);
            const bool bBefore = c.read(f.b);
            f.dispatcher->apply(&item, kSliceA);
            QCOMPARE(item.buttonState(c.id), aBefore);

            QVERIFY(f.dispatcher->click(c.id, kSliceA).isEmpty());
            QCOMPARE(c.read(f.a), !aBefore);
            QCOMPARE(c.read(f.b), bBefore);
            f.dispatcher->apply(&item, kSliceA);
            QCOMPARE(item.buttonState(c.id), !aBefore);

            QVERIFY(f.dispatcher->click(c.id, kSliceA).isEmpty());
            QCOMPARE(c.read(f.a), aBefore);
            QCOMPARE(c.read(f.b), bBefore);
            f.dispatcher->apply(&item, kSliceA);
            QCOMPARE(item.buttonState(c.id), aBefore);
        }
    }

    void aButtonClickReachesTheDispatcherThroughTheItem()
    {
        Fixture f;
        OtherButtonItem item;
        item.setRect(0.0f, 0.0f, 1.0f, 1.0f);
        f.dispatcher->apply(&item, kSliceA);
        QSignalSpy clicked(&item, &OtherButtonItem::otherButtonClicked);
        QSignalSpy refused(&item, &ButtonBoxItem::unavailableButtonClicked);
        clickShownButton(item, item.indexOf(Id::Anf));
        QCOMPARE(clicked.count(), 1);
        QCOMPARE(clicked.at(0).at(0).toInt(), int(Id::Anf));
        QCOMPARE(refused.count(), 0);
    }

    void bandButtonsChangeAndLightTheContainersSlice()
    {
        Fixture f;
        BandButtonItem bands;
        f.dispatcher->applyBand(&bands, kSliceA);
        QCOMPARE(bands.activeBand(), uiIndexFromBand(Band::Band20m));
        f.dispatcher->applyBand(&bands, kSliceB);
        QCOMPARE(bands.activeBand(), uiIndexFromBand(Band::Band80m));

        // Slice B is active; the container is on slice A.
        QVERIFY(f.dispatcher->clickBand(uiIndexFromBand(Band::Band40m), kSliceA).isEmpty());
        QCOMPARE(bandFromFrequency(f.a->frequency()), Band::Band40m);
        QCOMPARE(bandFromFrequency(f.b->frequency()), Band::Band80m);
        f.dispatcher->applyBand(&bands, kSliceA);
        QCOMPARE(bands.activeBand(), uiIndexFromBand(Band::Band40m));
        QVERIFY(bands.button(uiIndexFromBand(Band::Band40m)).on);
        QVERIFY(!bands.button(uiIndexFromBand(Band::Band20m)).on);

        // The band changed from elsewhere (the VFO): the buttons follow.
        f.a->setFrequency(21200000.0);
        f.dispatcher->applyBand(&bands, kSliceA);
        QCOMPARE(bands.activeBand(), uiIndexFromBand(Band::Band15m));
        QVERIFY(!bands.button(uiIndexFromBand(Band::Band40m)).on);
    }

    // ── A slice that is not open ────────────────────────────────────────

    void aContainerOnASliceThatIsNotOpenDoesNothing()
    {
        Fixture f;
        QVERIFY(f.model.sliceById(2) == nullptr);
        OtherButtonItem item;
        item.setRect(0.0f, 0.0f, 1.0f, 1.0f);
        f.dispatcher->apply(&item, kSliceC);

        const QString reason = ContainerButtonDispatcher::noSliceReason(kSliceC);
        QVERIFY(OperatorWording::isPlain(reason));
        QVERIFY(reason.contains(QStringLiteral("Slice C")));
        QVERIFY(!reason.contains(QStringLiteral("RX")));
        for (Id id : {Id::Anf, Id::Snb, Id::Mute, Id::Bin, Id::PeakHold, Id::Ctun}) {
            QVERIFY(!item.isButtonAvailable(id));
            QCOMPARE(item.buttonUnavailableReason(item.indexOf(id)), reason);
            QCOMPARE(f.dispatcher->click(id, kSliceC), reason);
        }
        QVERIFY(!f.a->anfEnabled());
        QVERIFY(!f.b->anfEnabled());

        // The click on the drawn button says why and changes nothing.
        QSignalSpy clicked(&item, &OtherButtonItem::otherButtonClicked);
        QSignalSpy refused(&item, &ButtonBoxItem::unavailableButtonClicked);
        clickShownButton(item, item.indexOf(Id::Anf));
        QCOMPARE(clicked.count(), 0);
        QCOMPARE(refused.count(), 1);
        QCOMPARE(refused.at(0).at(1).toString(), reason);

        // The slice's boxes are unavailable too, and the band click is
        // refused with the same reason.
        BandButtonItem bands;
        f.dispatcher->applyBand(&bands, kSliceC);
        QCOMPARE(bands.activeBand(), -1);
        QVERIFY(!bands.isButtonAvailable(0));
        QCOMPARE(f.dispatcher->clickBand(uiIndexFromBand(Band::Band40m), kSliceC), reason);
        QCOMPARE(bandFromFrequency(f.a->frequency()), Band::Band20m);
        ModeButtonItem modes;
        f.dispatcher->applySliceAvailability(&modes, kSliceC);
        QVERIFY(!modes.isButtonAvailable(0));
        f.dispatcher->applySliceAvailability(&modes, kSliceA);
        QVERIFY(modes.isButtonAvailable(0));
    }

    // Slice control plan Task 15 fix round 1: a slice another device
    // controls refuses every slice button with the window's reason (the
    // one the RX applet shows), in a remote window as in a hosting one.
    // Mute on that slice changes nothing.
    void aSliceAnotherDeviceControlsRefusesItsButtons()
    {
        Fixture f;
        const QString held = QStringLiteral("iPad controls this slice");
        ContainerButtonDispatcher::Hooks hooks;
        hooks.sliceRefusal = [&held](int sliceId) {
            return sliceId == 0 ? held : QString();
        };
        hooks.spectrumFor = [&f](SliceModel* s) -> SpectrumWidget* {
            return s == f.a ? &f.spectrumA : (s == f.b ? &f.spectrumB : nullptr);
        };
        ContainerButtonDispatcher dispatcher(&f.model, std::move(hooks));

        const bool mutedBefore = f.a->muted();
        QSignalSpy muted(f.a, &SliceModel::mutedChanged);
        QVERIFY(dispatcher.sliceFor(kSliceA) == nullptr);
        OtherButtonItem item;
        dispatcher.apply(&item, kSliceA);
        for (Id id : {Id::Anf, Id::Snb, Id::Mute, Id::Bin, Id::PeakHold, Id::Ctun}) {
            QVERIFY(!item.isButtonAvailable(id));
            QCOMPARE(item.buttonUnavailableReason(item.indexOf(id)), held);
            QCOMPARE(dispatcher.click(id, kSliceA), held);
        }
        QCOMPARE(f.a->muted(), mutedBefore);
        QCOMPARE(muted.count(), 0);
        QVERIFY(!f.a->anfEnabled());
        QCOMPARE(dispatcher.clickBand(uiIndexFromBand(Band::Band40m), kSliceA), held);
        QCOMPARE(bandFromFrequency(f.a->frequency()), Band::Band20m);
        ModeButtonItem modes;
        dispatcher.applySliceAvailability(&modes, kSliceA);
        QVERIFY(!modes.isButtonAvailable(0));

        // Slice B, which this window controls, still mutes.
        QVERIFY(dispatcher.click(Id::Mute, kSliceB).isEmpty());
        QVERIFY(f.b->muted());
    }

    // ── Transmit and global buttons ─────────────────────────────────────

    void transmitButtonsDoNothingWithNoRadio()
    {
        Fixture f;
        QVERIFY(!f.model.isConnected());
        OtherButtonItem item;
        f.dispatcher->apply(&item, kSliceA);
        const QString reason = ContainerButtonDispatcher::noRadioTransmitReason();
        QVERIFY(OperatorWording::isPlain(reason));
        for (Id id : {Id::Tun, Id::Mox, Id::TwoTon}) {
            QVERIFY(!item.isButtonAvailable(id));
            QCOMPARE(f.dispatcher->click(id, kSliceA), reason);
        }
        QVERIFY(!f.model.isTune());
        QVERIFY(!f.model.moxController()->isMox());
        QVERIFY(!f.model.twoToneController()->isActive());

        // With a radio they are there to use, as the TX applet's are.
        f.model.setConnectionStateForTest(ConnectionState::Connected);
        f.dispatcher->apply(&item, kSliceA);
        for (Id id : {Id::Tun, Id::Mox, Id::TwoTon}) {
            QVERIFY(item.isButtonAvailable(id));
        }
        f.model.setConnectionStateForTest(ConnectionState::Disconnected);
    }

    // Task 7 fix wave, I2: the container MOX button keys nothing while TX is
    // inhibited or the PA is tripped (Thetis disables chkMOX and PollPTT's
    // gate skips every source, console.cs:15341-15363 and 25470
    // [v2.10.3.15]).
    void moxButtonIsBlockedByTxInhibitAndPaTrip()
    {
        Fixture f;
        f.model.setConnectionStateForTest(ConnectionState::Connected);
        MoxController* mox = f.model.moxController();
        mox->setTimerIntervals(0, 0, 0, 0, 0, 0);
        mox->setMoxCheck([]() {
            return safety::BandPlanGuard::MoxCheckResult{true, QString()};
        });

        f.model.txInhibit().setEnabled(true);
        f.model.txInhibit().setUserIoReader([] { return true; });
        f.dispatcher->click(Id::Mox, kSliceA);
        QCoreApplication::processEvents();
        QVERIFY2(!mox->isMox(), "the container MOX button keyed while TX is inhibited");
        QVERIFY(!mox->isManualKey());
        f.model.txInhibit().setEnabled(false);

        f.model.handleGanymedeTrip(0x01);
        f.dispatcher->click(Id::Mox, kSliceA);
        QCoreApplication::processEvents();
        QVERIFY2(!mox->isMox(), "the container MOX button keyed while the PA is tripped");
        QVERIFY(!mox->isManualKey());
        f.model.resetGanymedePa();
        f.model.setConnectionStateForTest(ConnectionState::Disconnected);
    }

    // Task 7 fix wave, M8: the container MOX button is a manual key, as
    // TxApplet's is (Thetis chkMOX_Click sets _manual_mox and no PTT mode,
    // console.cs:29730-29747 [v2.10.3.15]): the mic neither releases it nor
    // takes its mode, and its own off clears the manual key.
    void containerMoxButtonIsAManualKey()
    {
        Fixture f;
        f.model.setConnectionStateForTest(ConnectionState::Connected);
        MoxController* mox = f.model.moxController();
        mox->setTimerIntervals(0, 0, 0, 0, 0, 0);
        mox->setMoxCheck([]() {
            return safety::BandPlanGuard::MoxCheckResult{true, QString()};
        });

        QCOMPARE(f.dispatcher->click(Id::Mox, kSliceA), QString());
        QCoreApplication::processEvents();
        QVERIFY(mox->isMox());
        QVERIFY(mox->isManualKey());
        QVERIFY(!mox->isManualMox());   // not the TUN button's flag
        QCOMPARE(mox->pttMode(), PttMode::None);

        for (int i = 0; i < 3; ++i) {
            mox->onMicPttFromRadio(true);
            mox->onMicPttFromRadio(false);
        }
        QCoreApplication::processEvents();
        QVERIFY2(mox->isMox(), "the mic released the container MOX button's key");
        QCOMPARE(mox->pttMode(), PttMode::None);

        QCOMPARE(f.dispatcher->click(Id::Mox, kSliceA), QString());
        QCoreApplication::processEvents();
        QVERIFY(!mox->isMox());
        QVERIFY(!mox->isManualKey());
        f.model.setConnectionStateForTest(ConnectionState::Disconnected);
    }

    void monAndNotchesActOnTheirTargets()
    {
        Fixture f;
        OtherButtonItem item;
        const bool mon = f.model.transmitModel().monEnabled();
        QVERIFY(f.dispatcher->click(Id::Mon, kSliceA).isEmpty());
        QCOMPARE(f.model.transmitModel().monEnabled(), !mon);
        f.dispatcher->apply(&item, kSliceA);
        QCOMPARE(item.buttonState(Id::Mon), !mon);
        QVERIFY(f.dispatcher->click(Id::Mon, kSliceA).isEmpty());
        QCOMPARE(f.model.transmitModel().monEnabled(), mon);

        NotchModel* notches = f.model.notchModel();
        QVERIFY(notches != nullptr);
        const bool mnf = notches->globalEnabled();
        QVERIFY(f.dispatcher->click(Id::Mnf, kSliceA).isEmpty());
        QCOMPARE(notches->globalEnabled(), !mnf);
        f.dispatcher->apply(&item, kSliceA);
        QCOMPARE(item.buttonState(Id::Mnf), !mnf);
        QVERIFY(f.dispatcher->click(Id::Mnf, kSliceA).isEmpty());
        QCOMPARE(notches->globalEnabled(), mnf);
    }

    void peakAndCtunActOnTheContainerSlicesPanadapter()
    {
        Fixture f;
        OtherButtonItem item;
        const bool peakB = f.spectrumB.peakHoldEnabled();
        const bool peakA = f.spectrumA.peakHoldEnabled();
        QVERIFY(f.dispatcher->click(Id::PeakHold, kSliceA).isEmpty());
        QCOMPARE(f.spectrumA.peakHoldEnabled(), !peakA);
        QCOMPARE(f.spectrumB.peakHoldEnabled(), peakB);
        f.dispatcher->apply(&item, kSliceA);
        QCOMPARE(item.buttonState(Id::PeakHold), !peakA);

        const bool ctunA = f.spectrumA.ctunEnabled();
        const bool ctunB = f.spectrumB.ctunEnabled();
        QVERIFY(f.dispatcher->click(Id::Ctun, kSliceA).isEmpty());
        QCOMPARE(f.spectrumA.ctunEnabled(), !ctunA);
        QCOMPARE(f.spectrumB.ctunEnabled(), ctunB);
        f.dispatcher->apply(&item, kSliceA);
        QCOMPARE(item.buttonState(Id::Ctun), !ctunA);
    }

    void vaxButtonsOpenAndCloseThisComputersVaxOutputs()
    {
        Fixture f;
        OtherButtonItem item;
        QVERIFY(!f.vax->isVaxBusOpen(1));
        QVERIFY(f.dispatcher->click(Id::Vac1, kSliceA).isEmpty());
        QVERIFY(f.vax->isVaxBusOpen(1));
        QVERIFY(!f.vax->isVaxBusOpen(2));
        QCOMPARE(AppSettings::instance().value(QStringLiteral("audio/Vax1/Enabled")).toString(),
                 QStringLiteral("True"));
        f.dispatcher->apply(&item, kSliceA);
        QVERIFY(item.buttonState(Id::Vac1));
        QVERIFY(!item.buttonState(Id::Vac2));

        QVERIFY(f.dispatcher->click(Id::Vac1, kSliceA).isEmpty());
        QVERIFY(!f.vax->isVaxBusOpen(1));
        QCOMPARE(AppSettings::instance().value(QStringLiteral("audio/Vax1/Enabled")).toString(),
                 QStringLiteral("False"));
    }

    // NereusSDR has no Power button (maintainer decision 2026-09-30). An
    // old Power id (a stale click) is unavailable and changes nothing, and
    // apply() never lights or enables it.
    void powerIsNotAButton()
    {
        Fixture f;
        OtherButtonItem item;
        f.dispatcher->apply(&item, kSliceA);
        QVERIFY(!item.isButtonShown(Id::Power));
        QVERIFY(!item.buttonState(Id::Power));
        const ContainerButtonDispatcher::State st = f.dispatcher->stateOf(Id::Power, kSliceA);
        QVERIFY(!st.available);
        QVERIFY(!st.on);
        const QString reason = f.dispatcher->click(Id::Power, kSliceA);
        QVERIFY(!reason.isEmpty());
        QVERIFY(OperatorWording::isPlain(reason));
        QCOMPARE(f.model.connectionState(), ConnectionState::Disconnected);
    }

    // A layout saved with a Power button loads without it; the other
    // buttons keep their saved visibility, and saving drops the Power bit.
    void savedPowerButtonIsDroppedOnLoad()
    {
        const uint32_t bits = (1u << int(Id::Power)) | (1u << int(Id::Mon))
            | (1u << int(Id::Tun)) | (1u << int(Id::Mox));
        OtherButtonItem item;
        QVERIFY(item.deserialize(QStringLiteral("OTHERBTNS|0.1|0.2|0.5|0.5|0|3|6|%1").arg(bits)));
        QVERIFY(!item.isButtonShown(Id::Power));
        QVERIFY(item.isButtonShown(Id::Mon));
        QVERIFY(item.isButtonShown(Id::Tun));
        QVERIFY(item.isButtonShown(Id::Mox));
        QVERIFY(!item.isButtonShown(Id::Anf));
        QCOMPARE(item.columns(), 6);
        const uint32_t kept = bits & ~(1u << int(Id::Power));
        QCOMPARE(item.visibleBits(), kept);
        QCOMPARE(item.serialize(), QStringLiteral("OTHERBTNS|0.1|0.2|0.5|0.5|0|3|6|%1").arg(kept));
    }

    void everyReasonIsPlain()
    {
        Fixture f;
        QVERIFY(OperatorWording::isPlain(ContainerButtonDispatcher::noRadioTransmitReason()));
        for (int rx = 1; rx <= 4; ++rx) {
            QVERIFY(OperatorWording::isPlain(ContainerButtonDispatcher::noSliceReason(rx)));
            QVERIFY(OperatorWording::isPlain(ContainerWidget::sliceNameForRxSource(rx)));
        }
        for (int rx : {kSliceA, kSliceC}) {
            for (Id id : kConnected) {
                const auto st = f.dispatcher->stateOf(id, rx);
                if (!st.available) {
                    QVERIFY2(OperatorWording::isPlain(st.reason), qPrintable(st.reason));
                }
            }
        }
    }
};

QTEST_MAIN(TstOtherButtonItem)
#include "tst_other_button_item.moc"
