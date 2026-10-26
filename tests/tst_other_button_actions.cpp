// =================================================================
// tests/tst_other_button_actions.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original test. It drives the container receive
// buttons through ContainerButtonDispatcher against a real RadioModel; no
// upstream logic is ported here.
//
// Container item parity Task 60: 31 receive buttons act on the container's
// slice (Thetis console.cs:51391-51614 [v2.10.3.15]):
//   - With slice A on the container and slice B active, each click
//     changes slice A only and lights as Thetis's clsOtherButtons does.
//   - NR cycles off, NR1-NR4, off; a NereusSDR-only reducer goes to off
//     and names itself on the NR button. NB cycles off, NB, NB2.
//   - A refused NR3 changes nothing and says the slice's reason.
//   - AF, BAL and AGC steps clamp at the ends.
//   - A container on a slice that is not open shows every receive button
//     unavailable with noSliceReason, and a click changes nothing.
//   - Mute All skips slices this window may not change.
//   - MNF+ adds a notch on the container's slice, and none while
//     transmitting.
//   - None keys the radio.
//   - Each receive button is offered, and its editor row is enabled.
//   - A settings preview's click never reaches the dispatcher.
//
// Container item parity Task 61: 20 transmit setting buttons call the TX
// applet's setters (Thetis console.cs:51486-51579 [v2.10.3.15]):
//   - In a local window each click sets the transmit model, the toggles
//     light from it and the steps clamp to their sliders' ranges.
//   - In a remote window whose gate refuses, each is unavailable with that
//     gate's reason (VOX with its VOX line reason) and a click changes
//     nothing.
//   - A remote TUN+ reaches the Core, whose transmit band's tune power
//     rises by 5.
//   - None keys the radio; a preview's click goes nowhere.
//
// Container item parity Task 62: 12 display and window buttons (Thetis
// console.cs:51498-51618 [v2.10.3.15]):
//   - NF, Peak Blobs, Cur Info, Fill and A-Peak toggle the panadapter that
//     shows the container's slice, not another pan, and light from it;
//     Peak Blobs and A-Peak are saved as that pan's own value. ZTB zooms
//     that pan. With no pan showing the slice they say so.
//   - Spots is saved and every pan follows.
//   - MAF+ and MAF- move the master volume through its slider, saved and
//     clamped to 0..1.
//   - Setup, Diversity and Linearity open their windows; Linearity follows
//     its menu entry's gate.
//   - None keys; a preview's click toggles no pan and opens no window.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-26  J.J. Boyd / KG4VCF  Container item parity Task 60.
//                                    AI-assisted via Anthropic Claude
//                                    Code.
//   2026-10-26  J.J. Boyd / KG4VCF  Container item parity Task 61: the
//                                    transmit setting buttons; the fixture
//                                    drops RadioModel's "TX frequency NOT
//                                    pushed" warning, which tuning with no
//                                    radio raises. AI-assisted via
//                                    Anthropic Claude Code.
//   2026-10-26  J.J. Boyd / KG4VCF  Container item parity Task 62: the
//                                    display and window buttons; the
//                                    fixture has a panadapter for each
//                                    slice and counts the windows opened.
//                                    AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QCheckBox>
#include <QDir>
#include <QImage>
#include <QPainter>
#include <QSignalSpy>
#include <QSlider>
#include <QSplitter>
#include <QTemporaryDir>

#include "core/AppSettings.h"
#include "core/BoardCapabilities.h"
#include "core/ConnectionState.h"
#include "core/ControlRanges.h"
#include "core/HpsdrModel.h"
#include "core/MoxController.h"
#include "core/safety/TxRefusal.h"
#include "core/session/IStationLink.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "core/settings/PanDisplayKeys.h"
#include "core/settings/SettingsProxy.h"
#include "fakes/LoopbackTransport.h"
#include "fakes/UpgradedCoreToken.h"
#include "gui/SpectrumWidget.h"
#include "gui/UnbuiltFeatures.h"
#include "gui/containers/ContainerButtonDispatcher.h"
#include "gui/containers/ContainerContentHost.h"
#include "gui/containers/ContainerContentRegistry.h"
#include "gui/containers/ContainerManager.h"
#include "gui/containers/ContainerPreviewWidget.h"
#include "gui/containers/ContainerSettingsDialog.h"
#include "gui/containers/ContainerWidget.h"
#include "gui/containers/ContainerWorkspaceStore.h"
#include "gui/containers/meter_property_editors/OtherButtonItemEditor.h"
#include "gui/meters/MeterPoller.h"
#include "gui/meters/MeterWidget.h"
#include "gui/meters/OtherButtonItem.h"
#include "gui/widgets/MasterOutputWidget.h"
#include "models/Band.h"
#include "models/BandPlanManager.h"
#include "models/NotchModel.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

#include <functional>
#include <memory>

using namespace NereusSDR;
using Id = OtherButtonItem::ButtonId;

namespace {

constexpr int kSliceA = 1;  // ContainerWidget::rxSource() for slice A
constexpr int kSliceC = 3;  // never opened here
constexpr int kSize = 600;

// The 31 receive buttons of Task 60, in Thetis id order.
const QList<Id> kReceive = {
    Id::Nr, Id::Nr1, Id::Nr2, Id::Nr3, Id::Nr4, Id::Nb, Id::Nb1, Id::Nb2, Id::MnfPlus,
    Id::Rit, Id::Rit0, Id::Xit, Id::Xit0, Id::LockA, Id::TuneStepUp, Id::TuneStepDown,
    Id::MuteAll, Id::SqlVsql, Id::AfP5, Id::AfM5, Id::BalP5, Id::BalM5,
    Id::AgcFixed, Id::AgcLong, Id::AgcSlow, Id::AgcMedium, Id::AgcFast, Id::AgcCustom,
    Id::AgcAuto, Id::AgcP5, Id::AgcM5,
};

// The 20 transmit setting buttons of Task 61, in Thetis id order.
const QList<Id> kTransmitSettings = {
    Id::Comp, Id::Vox, Id::Dexp, Id::TxEq, Id::Cfc, Id::CfcEq,
    Id::MicP1, Id::MicM1, Id::CompP1, Id::CompM1, Id::VoxP1, Id::VoxM1,
    Id::Leveler, Id::PhaseRot,
    Id::DriveP5, Id::DriveM5, Id::TuneP5, Id::TuneM5, Id::Drive0, Id::Tun0,
};

// The 12 display and window buttons of Task 62, in Thetis id order.
const QList<Id> kDisplayAndWindow = {
    Id::Nf, Id::PeakBlobs, Id::CursorInfo, Id::Spots, Id::FillSpectrum, Id::Ztb,
    Id::ActivePeak, Id::MafP5, Id::MafM5, Id::FormSetup, Id::FormDiversity, Id::FormLinearity,
};

// Task 62: the panadapter toggles and their getters.
struct PanToggle {
    Id id;
    QString keyBase;  // the pan's saved display key
    bool (SpectrumWidget::*get)() const;
};
const QList<PanToggle> kPanToggles = {
    {Id::Nf, QStringLiteral("DisplayShowNoiseFloor"), &SpectrumWidget::showNoiseFloor},
    {Id::PeakBlobs, QStringLiteral("DisplayPeakBlobsEnabled"), &SpectrumWidget::peakBlobsEnabled},
    {Id::CursorInfo, QStringLiteral("DisplayShowCursorFreq"), &SpectrumWidget::cursorFreqVisible},
    {Id::FillSpectrum, QStringLiteral("DisplayPanFill"), &SpectrumWidget::panFillEnabled},
    {Id::ActivePeak, QStringLiteral("DisplayActivePeakHoldEnabled"),
     &SpectrumWidget::activePeakHoldEnabled},
};

// Everything a display button may change on a pan.
struct PanSnapshot {
    QList<bool> toggles;
    bool spots;
    double centre;
    double bandwidth;

    static PanSnapshot of(const SpectrumWidget& w)
    {
        PanSnapshot snap{{}, w.showSpots(), w.centerFrequency(), w.bandwidth()};
        for (const PanToggle& t : kPanToggles) { snap.toggles << (w.*t.get)(); }
        return snap;
    }
    bool operator==(const PanSnapshot& o) const
    {
        return toggles == o.toggles && spots == o.spots && centre == o.centre
            && bandwidth == o.bandwidth;
    }
};

// RadioModel warns that it cannot push the transmit frequency each time the
// transmit slice is tuned with no radio connected (pushTxFrequencyFromTxSlice).
// These tests tune with no radio on purpose; every other message passes.
class QuietUnconnectedTxFrequency {
public:
    QuietUnconnectedTxFrequency() { s_previous = qInstallMessageHandler(&drop); }
    ~QuietUnconnectedTxFrequency() { qInstallMessageHandler(s_previous); }
    QuietUnconnectedTxFrequency(const QuietUnconnectedTxFrequency&) = delete;
    QuietUnconnectedTxFrequency& operator=(const QuietUnconnectedTxFrequency&) = delete;

private:
    static void drop(QtMsgType type, const QMessageLogContext& context, const QString& message)
    {
        if (type == QtWarningMsg
            && message.startsWith(QStringLiteral("TX frequency NOT pushed: no connection yet"))) {
            return;
        }
        if (s_previous) { s_previous(type, context, message); }
    }
    static inline QtMessageHandler s_previous = nullptr;
};

// Task 62: a SpectrumWidget built on the offscreen platform says once per
// GPU child that the platform has no QRhi; the fixture's two pans would say
// it for every case. Only that line is dropped; every other message passes.
class QuietOffscreenRhi {
public:
    QuietOffscreenRhi() { s_previous = qInstallMessageHandler(&drop); }
    ~QuietOffscreenRhi() { qInstallMessageHandler(s_previous); }
    QuietOffscreenRhi(const QuietOffscreenRhi&) = delete;
    QuietOffscreenRhi& operator=(const QuietOffscreenRhi&) = delete;

private:
    static void drop(QtMsgType type, const QMessageLogContext& context, const QString& message)
    {
        if (type == QtWarningMsg
            && message == QStringLiteral("QRhiWidget: QRhi is not supported on this platform.")) {
            return;
        }
        if (s_previous) { s_previous(type, context, message); }
    }
    static inline QtMessageHandler s_previous = nullptr;
};

// Everything a transmit setting button may change.
struct TransmitSnapshot {
    bool cpdr, vox, dexp, txEq, cfc, cfcEq, leveler, phaseRot;
    int mic, cpdrLevel, voxThreshold, power, tune20m, tuneTx;

    static TransmitSnapshot of(const TransmitModel& t)
    {
        return {t.cpdrOn(), t.voxEnabled(), t.dexpEnabled(), t.txEqEnabled(), t.cfcEnabled(),
                t.cfcPostEqEnabled(), t.txLevelerOn(), t.phaseRotatorEnabled(), t.micGainDb(),
                t.cpdrLevelDb(), t.voxThresholdDb(), t.power(), t.tunePowerForBand(Band::Band20m),
                t.tunePowerForTxBand()};
    }
    bool operator==(const TransmitSnapshot& o) const
    {
        return cpdr == o.cpdr && vox == o.vox && dexp == o.dexp && txEq == o.txEq && cfc == o.cfc
            && cfcEq == o.cfcEq && leveler == o.leveler && phaseRot == o.phaseRot && mic == o.mic
            && cpdrLevel == o.cpdrLevel && voxThreshold == o.voxThreshold && power == o.power
            && tune20m == o.tune20m && tuneTx == o.tuneTx;
    }
};

// A Core with an HL2 on 40 m and one remote window, linked in memory, the
// loopback session of tst_transmit_model_properties.cpp.
struct LoopbackSession {
    explicit LoopbackSession(const QString& securityDir, QObject* parent)
        : settings(settingsDir.filePath(QStringLiteral("NereusSDR.settings")))
    {
        // Both ends' settings as CoreInit::initialize() leaves them, so the
        // session finds no schema skew.
        settings.setValue(QStringLiteral("SettingsSchemaVersion"), kSchemaVersion);
        AppSettings::instance().setValue(QStringLiteral("SettingsSchemaVersion"), kSchemaVersion);
        core = std::make_unique<RadioModel>();
        core->setBoardForTest(HPSDRHW::HermesLite);
        core->setHpsdrModelForTest(HPSDRModel::HERMESLITE);
        core->setConnectionStateForTest(ConnectionState::Connected);
        core->addSlice(QStringLiteral("pan-0"));
        server = std::make_unique<StationServer>(
            core.get(), settings, NereusSDR::Test::seedUpgradedCoreToken(securityDir));
        client = std::make_unique<StationClient>(&window, &proxy);
        coreEnd = new NereusSDR::Test::LoopbackTransport(QStringLiteral("station-end"), parent);
        windowEnd = new NereusSDR::Test::LoopbackTransport(QStringLiteral("client-end"), parent);
        coreEnd->linkTo(windowEnd);
    }
    bool connect()
    {
        // A remote window's RadioModel takes the radio's name, model and
        // version from its own connection, not from the Core's snapshot;
        // StationClient says so once per property.
        for (const char* property : {"name", "model", "version"}) {
            QTest::ignoreMessage(QtWarningMsg,
                qPrintable(QStringLiteral("No way to apply station value for \"RadioModel.%1\" "
                                          "-- this property will read stale on this client")
                               .arg(QLatin1String(property))));
        }
        QSignalSpy completed(client.get(), &StationClient::handshakeComplete);
        client->startSession(windowEnd, server->token());
        server->acceptTransport(coreEnd);
        return completed.wait(5000) || completed.count() == 1;
    }

    QTemporaryDir settingsDir;
    AppSettings settings;
    std::unique_ptr<RadioModel> core;
    std::unique_ptr<StationServer> server;
    RadioModel window{RadioModel::Role::Remote};
    SettingsProxy proxy;
    std::unique_ptr<StationClient> client;
    NereusSDR::Test::LoopbackTransport* coreEnd = nullptr;
    NereusSDR::Test::LoopbackTransport* windowEnd = nullptr;
    static inline const QString kSchemaVersion = QStringLiteral("7");
};

// Everything a receive button may change on a slice.
struct SliceSnapshot {
    NrSlot nr;
    NbMode nb;
    bool rit;
    int ritHz;
    bool xit;
    int xitHz;
    bool locked;
    int stepHz;
    bool muted;
    bool ssql;
    int af;
    double pan;
    AGCMode agc;
    bool autoAgc;
    int agcThreshold;

    static SliceSnapshot of(const SliceModel& s)
    {
        return {s.activeNr(), s.nbMode(), s.ritEnabled(), s.ritHz(), s.xitEnabled(), s.xitHz(),
                s.locked(), s.stepHz(), s.muted(), s.ssqlEnabled(), s.afGain(), s.audioPan(),
                s.agcMode(), s.autoAgcEnabled(), s.agcThreshold()};
    }
    bool operator==(const SliceSnapshot& o) const
    {
        return nr == o.nr && nb == o.nb && rit == o.rit && ritHz == o.ritHz && xit == o.xit
            && xitHz == o.xitHz && locked == o.locked && stepHz == o.stepHz && muted == o.muted
            && ssql == o.ssql && af == o.af && qFuzzyCompare(pan + 2.0, o.pan + 2.0)
            && agc == o.agc && autoAgc == o.autoAgc && agcThreshold == o.agcThreshold;
    }
};

OtherButtonItem* firstGrid(const QList<MeterWidget*>& meters)
{
    for (MeterWidget* meter : meters) {
        for (MeterItem* item : meter->items()) {
            if (auto* other = qobject_cast<OtherButtonItem*>(item)) { return other; }
        }
    }
    return nullptr;
}

} // namespace

class TstOtherButtonActions : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_securityDir;

    // A transmit setting button's row: how to set the model up, what the
    // click must have done, and whether it lights after.
    struct TxRow {
        Id id;
        std::function<void(TransmitModel&)> prepare;
        std::function<bool(const TransmitModel&)> changed;
        bool litAfter;
    };

    // The tune power the TX applet shows before the transmit band is known
    // (TxApplet::shownTunePower): slice A's band, 20 m, in the fixture.
    static int shownTunePower(const TransmitModel& t)
    {
        return t.tuneTxBandKnown() ? t.tunePowerForTxBand() : t.tunePowerForBand(Band::Band20m);
    }
    static void setShownTunePower(TransmitModel& t, int watts)
    {
        if (!t.setTunePowerForTxBand(watts)) { t.setTunePowerForBand(Band::Band20m, watts); }
    }

    static QList<TxRow> txRows()
    {
        using T = TransmitModel;
        const auto toggle = [](Id id, std::function<void(T&, bool)> set,
                               std::function<bool(const T&)> get) {
            return TxRow{id, [set](T& t) { set(t, false); }, get, true};
        };
        return {
            toggle(Id::Comp, &T::setCpdrOn, &T::cpdrOn),
            toggle(Id::Vox, &T::setVoxEnabled, &T::voxEnabled),
            toggle(Id::Dexp, &T::setDexpEnabled, &T::dexpEnabled),
            toggle(Id::TxEq, &T::setTxEqEnabled, &T::txEqEnabled),
            toggle(Id::Cfc, &T::setCfcEnabled, &T::cfcEnabled),
            toggle(Id::CfcEq, &T::setCfcPostEqEnabled, &T::cfcPostEqEnabled),
            {Id::MicP1, [](T& t) { t.setMicGainDb(0); },
             [](const T& t) { return t.micGainDb() == 1; }, false},
            {Id::MicM1, [](T& t) { t.setMicGainDb(0); },
             [](const T& t) { return t.micGainDb() == -1; }, false},
            {Id::CompP1, [](T& t) { t.setCpdrLevelDb(10); },
             [](const T& t) { return t.cpdrLevelDb() == 11; }, false},
            {Id::CompM1, [](T& t) { t.setCpdrLevelDb(10); },
             [](const T& t) { return t.cpdrLevelDb() == 9; }, false},
            {Id::VoxP1, [](T& t) { t.setVoxThresholdDb(-20); },
             [](const T& t) { return t.voxThresholdDb() == -19; }, false},
            {Id::VoxM1, [](T& t) { t.setVoxThresholdDb(-20); },
             [](const T& t) { return t.voxThresholdDb() == -21; }, false},
            toggle(Id::Leveler, &T::setTxLevelerOn, &T::txLevelerOn),
            toggle(Id::PhaseRot, &T::setPhaseRotatorEnabled, &T::phaseRotatorEnabled),
            {Id::DriveP5, [](T& t) { t.setPower(50); },
             [](const T& t) { return t.power() == 55; }, false},
            {Id::DriveM5, [](T& t) { t.setPower(50); },
             [](const T& t) { return t.power() == 45; }, false},
            {Id::TuneP5, [](T& t) { setShownTunePower(t, 30); },
             [](const T& t) { return shownTunePower(t) == 35; }, false},
            {Id::TuneM5, [](T& t) { setShownTunePower(t, 30); },
             [](const T& t) { return shownTunePower(t) == 25; }, false},
            {Id::Drive0, [](T& t) { t.setPower(50); },
             [](const T& t) { return t.power() == 0; }, false},
            {Id::Tun0, [](T& t) { setShownTunePower(t, 30); },
             [](const T& t) { return shownTunePower(t) == 0; }, false},
        };
    }

    // The editor lists each of `ids` enabled, under `captions`.
    static void checkOfferedWithEnabledEditorRows(const QList<Id>& ids, const QStringList& captions)
    {
        OtherButtonItem item;
        OtherButtonItemEditor editor;
        editor.setItem(&item);
        for (Id id : ids) {
            QVERIFY2(item.isOffered(id), qPrintable(QString::number(int(id))));
            QVERIFY(!OtherButtonItem::unbuiltFeatureFor(id).has_value());
            // Hidden on a new grid, so an earlier layout looks the same.
            QVERIFY(!item.isButtonVisible(id));
            auto* check = editor.findChild<QCheckBox*>(
                QStringLiteral("otherVisible_%1").arg(int(id)));
            QVERIFY2(check, qPrintable(QString::number(int(id))));
            QVERIFY(check->isEnabled());
            QVERIFY(!check->toolTip().contains(UnbuiltFeatures::notBuiltReason()));
            check->setChecked(true);
            QVERIFY(item.isButtonShown(id));
        }
        QCOMPARE(ids.size(), captions.size());
        for (int i = 0; i < ids.size(); ++i) {
            QCOMPARE(item.buttonLabel(ids[i]), captions[i]);
        }
    }

    // A local model with slices A and B, B active, and no radio (the
    // fixture of tst_other_button_item.cpp).
    struct Fixture {
        // First, so it outlives the model's own messages.
        QuietUnconnectedTxFrequency quiet;
        // Task 62: and the pans' offscreen QRhi line.
        QuietOffscreenRhi quietRhi;
        RadioModel model;
        SliceModel* a{nullptr};
        SliceModel* b{nullptr};
        // Task 62: the panadapter showing each slice.
        SpectrumWidget spectrumA;
        SpectrumWidget spectrumB;
        // Slice ids this window may not change (another device's).
        QSet<int> refused;
        // Task 62: slice ids no panadapter shows.
        QSet<int> noPan;
        // Task 62: the windows opened and the volume steps asked for; the
        // master volume widget, when a test has one.
        int setups{0};
        int diversities{0};
        int linearities{0};
        QString linearityRefusal;
        QList<double> volumeSteps;
        MasterOutputWidget* master{nullptr};
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

            ContainerButtonDispatcher::Hooks hooks;
            hooks.transmitPermitted = [] { return true; };
            hooks.sliceRefusal = [this](int sliceId) {
                return refused.contains(sliceId)
                    ? QStringLiteral("Another device controls this slice.") : QString();
            };
            hooks.spectrumFor = [this](SliceModel* s) -> SpectrumWidget* {
                if (!s || noPan.contains(s->sliceIndex())) { return nullptr; }
                return s == a ? &spectrumA : (s == b ? &spectrumB : nullptr);
            };
            // MainWindow's Spots hooks, over this window's two pans.
            hooks.spotsShown = [] { return ContainerButtonDispatcher::spotsShownSetting(); };
            hooks.setSpotsShown = [this](bool on) {
                ContainerButtonDispatcher::showSpotsOnPans(on, {&spectrumA, &spectrumB});
            };
            hooks.stepMasterVolume = [this](double delta) {
                volumeSteps << delta;
                if (master) { master->stepVolume(delta); }
            };
            hooks.openSetup = [this] { ++setups; };
            hooks.openDiversity = [this] { ++diversities; };
            hooks.openLinearity = [this] { ++linearities; };
            hooks.linearityRefusal = [this] { return linearityRefusal; };
            dispatcher = std::make_unique<ContainerButtonDispatcher>(&model, std::move(hooks));
        }
    };

    struct Row {
        Id id;
        std::function<void(SliceModel&)> prepare;
        std::function<bool(const SliceModel&)> changed;
        bool litAfter;
    };

    static QList<Row> rows()
    {
        using S = SliceModel;
        const auto agcRow = [](Id id, AGCMode mode, AGCMode from) {
            return Row{id, [from](S& s) { s.setAgcMode(from); },
                       [mode](const S& s) { return s.agcMode() == mode; }, true};
        };
        return {
            {Id::Nr, [](S& s) { s.setActiveNr(NrSlot::Off); },
             [](const S& s) { return s.activeNr() == NrSlot::NR1; }, true},
            {Id::Nr1, [](S& s) { s.setActiveNr(NrSlot::Off); },
             [](const S& s) { return s.activeNr() == NrSlot::NR1; }, true},
            {Id::Nr2, [](S& s) { s.setActiveNr(NrSlot::NR1); },
             [](const S& s) { return s.activeNr() == NrSlot::NR2; }, true},
            {Id::Nr3, [](S& s) { s.setActiveNr(NrSlot::Off); },
             [](const S& s) { return s.activeNr() == NrSlot::NR3; }, true},
            {Id::Nr4, [](S& s) { s.setActiveNr(NrSlot::Off); },
             [](const S& s) { return s.activeNr() == NrSlot::NR4; }, true},
            {Id::Nb, [](S& s) { s.setNbMode(NbMode::Off); },
             [](const S& s) { return s.nbMode() == NbMode::NB; }, true},
            {Id::Nb1, [](S& s) { s.setNbMode(NbMode::NB2); },
             [](const S& s) { return s.nbMode() == NbMode::NB; }, true},
            {Id::Nb2, [](S& s) { s.setNbMode(NbMode::Off); },
             [](const S& s) { return s.nbMode() == NbMode::NB2; }, true},
            {Id::Rit, [](S& s) { s.setRitEnabled(false); },
             [](const S& s) { return s.ritEnabled(); }, true},
            {Id::Rit0, [](S& s) { s.setRitHz(500); },
             [](const S& s) { return s.ritHz() == 0; }, false},
            {Id::Xit, [](S& s) { s.setXitEnabled(false); },
             [](const S& s) { return s.xitEnabled(); }, true},
            {Id::Xit0, [](S& s) { s.setXitHz(-300); },
             [](const S& s) { return s.xitHz() == 0; }, false},
            {Id::LockA, [](S& s) { s.setLocked(false); },
             [](const S& s) { return s.locked(); }, true},
            {Id::TuneStepUp, [](S& s) { s.setStepHz(100); },
             [](const S& s) { return s.stepHz() > 100; }, false},
            {Id::TuneStepDown, [](S& s) { s.setStepHz(100); },
             [](const S& s) { return s.stepHz() < 100; }, false},
            {Id::SqlVsql, [](S& s) { s.setSsqlEnabled(false); },
             [](const S& s) { return s.ssqlEnabled(); }, true},
            {Id::AfP5, [](S& s) { s.setAfGain(50); },
             [](const S& s) { return s.afGain() == 55; }, false},
            {Id::AfM5, [](S& s) { s.setAfGain(50); },
             [](const S& s) { return s.afGain() == 45; }, false},
            {Id::BalP5, [](S& s) { s.setAudioPan(0.0); },
             [](const S& s) { return qFuzzyCompare(s.audioPan(), 0.1); }, false},
            {Id::BalM5, [](S& s) { s.setAudioPan(0.0); },
             [](const S& s) { return qFuzzyCompare(s.audioPan(), -0.1); }, false},
            agcRow(Id::AgcFixed, AGCMode::Off, AGCMode::Med),
            agcRow(Id::AgcLong, AGCMode::Long, AGCMode::Med),
            agcRow(Id::AgcSlow, AGCMode::Slow, AGCMode::Med),
            agcRow(Id::AgcMedium, AGCMode::Med, AGCMode::Fast),
            agcRow(Id::AgcFast, AGCMode::Fast, AGCMode::Med),
            agcRow(Id::AgcCustom, AGCMode::Custom, AGCMode::Med),
            {Id::AgcAuto, [](S& s) { s.setAutoAgcEnabled(false); },
             [](const S& s) { return s.autoAgcEnabled(); }, true},
            {Id::AgcP5, [](S& s) { s.setAgcThreshold(-60); },
             [](const S& s) { return s.agcThreshold() == -55; }, false},
            {Id::AgcM5, [](S& s) { s.setAgcThreshold(-60); },
             [](const S& s) { return s.agcThreshold() == -65; }, false},
        };
    }

private slots:
    void initTestCase() { QVERIFY(m_securityDir.isValid()); }

    void init()
    {
        UnbuiltFeatures::resetForTest();
        AppSettings::instance().clear();
    }
    void cleanup() { UnbuiltFeatures::resetForTest(); }

    // Acceptance: every row, with slice A on the container and slice B
    // active, changes slice A only and lights as the row says.
    void everyRowChangesTheContainersSliceOnly()
    {
        for (const Row& row : rows()) {
            Fixture f;
            row.prepare(*f.a);
            row.prepare(*f.b);
            const SliceSnapshot bBefore = SliceSnapshot::of(*f.b);
            const auto before = f.dispatcher->stateOf(row.id, kSliceA);
            QVERIFY2(before.available, qPrintable(QString::number(int(row.id))));

            QCOMPARE(f.dispatcher->click(row.id, kSliceA), QString());
            QVERIFY2(row.changed(*f.a), qPrintable(QString::number(int(row.id))));
            QVERIFY2(SliceSnapshot::of(*f.b) == bBefore, qPrintable(QString::number(int(row.id))));
            QCOMPARE(f.model.activeSlice(), f.b);
            QCOMPARE(f.dispatcher->stateOf(row.id, kSliceA).on, row.litAfter);
            // Slice B's container lights from slice B, not A.
            QVERIFY2(!f.dispatcher->stateOf(row.id, kSliceA + 1).on,
                     qPrintable(QString::number(int(row.id))));
        }
    }

    // The toggles go back on a second click; the select buttons go off.
    void secondClicksTurnThemOff()
    {
        Fixture f;
        for (Id id : {Id::Nr1, Id::Nr2, Id::Nr3, Id::Nr4}) {
            f.dispatcher->click(id, kSliceA);
            QVERIFY(f.dispatcher->stateOf(id, kSliceA).on);
            f.dispatcher->click(id, kSliceA);
            QCOMPARE(f.a->activeNr(), NrSlot::Off);
            QVERIFY(!f.dispatcher->stateOf(id, kSliceA).on);
        }
        for (Id id : {Id::Nb1, Id::Nb2}) {
            f.dispatcher->click(id, kSliceA);
            QVERIFY(f.dispatcher->stateOf(id, kSliceA).on);
            f.dispatcher->click(id, kSliceA);
            QCOMPARE(f.a->nbMode(), NbMode::Off);
        }
        for (Id id : {Id::Rit, Id::Xit, Id::LockA, Id::SqlVsql, Id::AgcAuto}) {
            f.dispatcher->click(id, kSliceA);
            QVERIFY(f.dispatcher->stateOf(id, kSliceA).on);
            f.dispatcher->click(id, kSliceA);
            QVERIFY(!f.dispatcher->stateOf(id, kSliceA).on);
        }
        // Only the selected AGC mode lights.
        f.dispatcher->click(Id::AgcSlow, kSliceA);
        for (Id id : {Id::AgcFixed, Id::AgcLong, Id::AgcSlow, Id::AgcMedium, Id::AgcFast,
                      Id::AgcCustom}) {
            QCOMPARE(f.dispatcher->stateOf(id, kSliceA).on, id == Id::AgcSlow);
        }
        // Momentary buttons never light.
        for (Id id : {Id::Rit0, Id::Xit0, Id::TuneStepUp, Id::TuneStepDown, Id::AfP5, Id::AfM5,
                      Id::BalP5, Id::BalM5, Id::AgcP5, Id::AgcM5, Id::MnfPlus}) {
            QVERIFY(!f.dispatcher->stateOf(id, kSliceA).on);
        }
    }

    // NR cycles as Thetis incrementNR (console.cs:43327-43339); the NR
    // button names the reducer (MeterManager.cs:8700). A NereusSDR-only
    // reducer goes to off and names itself.
    void nrAndNbCycleAndNameTheReducer()
    {
        Fixture f;
        QCOMPARE(f.dispatcher->stateOf(Id::Nr, kSliceA).label, QStringLiteral("NR"));
        const QList<QPair<NrSlot, QString>> cycle = {
            {NrSlot::NR1, QStringLiteral("NR")}, {NrSlot::NR2, QStringLiteral("NR2")},
            {NrSlot::NR3, QStringLiteral("NR3")}, {NrSlot::NR4, QStringLiteral("NR4")},
            {NrSlot::Off, QStringLiteral("NR")}};
        for (const auto& step : cycle) {
            QCOMPARE(f.dispatcher->click(Id::Nr, kSliceA), QString());
            QCOMPARE(f.a->activeNr(), step.first);
            const auto st = f.dispatcher->stateOf(Id::Nr, kSliceA);
            QCOMPARE(st.label, step.second);
            QCOMPARE(st.on, step.first != NrSlot::Off);
        }
        // This computer may not run every NereusSDR-only reducer; take them
        // all here, as a Core that has them would.
        f.a->setNrSelectionApplier([](NrSlot, QString*) { return true; });
        for (NrSlot own : {NrSlot::DFNR, NrSlot::BNR, NrSlot::MNR, NrSlot::NNR}) {
            f.a->setActiveNr(own);
            QCOMPARE(f.a->activeNr(), own);
            const QString name = OtherButtonItem::nrLabel(own);
            QCOMPARE(f.dispatcher->stateOf(Id::Nr, kSliceA).label, name);
            QVERIFY(f.dispatcher->stateOf(Id::Nr, kSliceA).on);
            f.dispatcher->click(Id::Nr, kSliceA);
            QCOMPARE(f.a->activeNr(), NrSlot::Off);
        }
        QCOMPARE(OtherButtonItem::nrLabel(NrSlot::DFNR), QStringLiteral("DFNR"));
        QCOMPARE(OtherButtonItem::nrLabel(NrSlot::NNR), QStringLiteral("NNR"));

        const QList<QPair<NbMode, QString>> nb = {
            {NbMode::NB, QStringLiteral("NB")}, {NbMode::NB2, QStringLiteral("NB2")},
            {NbMode::Off, QStringLiteral("NB")}};
        for (const auto& step : nb) {
            f.dispatcher->click(Id::Nb, kSliceA);
            QCOMPARE(f.a->nbMode(), step.first);
            QCOMPARE(f.dispatcher->stateOf(Id::Nb, kSliceA).label, step.second);
            QCOMPARE(f.dispatcher->stateOf(Id::Nb, kSliceA).on, step.first != NbMode::Off);
            // NB1 / NB2 light with the blanker they name.
            QCOMPARE(f.dispatcher->stateOf(Id::Nb1, kSliceA).on, step.first == NbMode::NB);
            QCOMPARE(f.dispatcher->stateOf(Id::Nb2, kSliceA).on, step.first == NbMode::NB2);
        }

        // apply() puts the caption on the grid; it moves with the id and is
        // never saved.
        OtherButtonItem item;
        f.a->setActiveNr(NrSlot::NR3);
        f.dispatcher->apply(&item, kSliceA);
        QCOMPARE(item.buttonLabel(Id::Nr), QStringLiteral("NR3"));
        const QString saved = item.serialize();
        f.a->setActiveNr(NrSlot::Off);
        f.dispatcher->apply(&item, kSliceA);
        QCOMPARE(item.buttonLabel(Id::Nr), QStringLiteral("NR"));
        QCOMPARE(item.serialize(), saved);
        // A slice that is not open shows the button's own caption.
        f.a->setActiveNr(NrSlot::NR2);
        f.dispatcher->apply(&item, kSliceA);
        QCOMPARE(item.buttonLabel(Id::Nr), QStringLiteral("NR2"));
        f.dispatcher->apply(&item, kSliceC);
        QCOMPARE(item.buttonLabel(Id::Nr), QStringLiteral("NR"));
        QCOMPARE(item.buttonLabel(Id::LockA), QStringLiteral("Lock"));
    }

    // Acceptance: NR3 refused changes nothing and shows the slice's
    // nrSelectionRefused reason.
    void aRefusedNr3ChangesNothingAndSaysWhy()
    {
        Fixture f;
        const QString why = QStringLiteral("NR3 needs its noise model on this Core.");
        f.a->setNrSelectionApplier([why](NrSlot slot, QString* reason) {
            if (slot == NrSlot::NR3) {
                *reason = why;
                return false;
            }
            return true;
        });
        f.a->setActiveNr(NrSlot::NR2);
        QSignalSpy changed(f.a, &SliceModel::activeNrChanged);
        QCOMPARE(f.dispatcher->click(Id::Nr3, kSliceA), why);
        QCOMPARE(f.a->activeNr(), NrSlot::NR2);
        // The NR button's cycle from NR2 is refused the same way.
        QCOMPARE(f.dispatcher->click(Id::Nr, kSliceA), why);
        QCOMPARE(f.a->activeNr(), NrSlot::NR2);
        QCOMPARE(changed.count(), 0);
        QVERIFY(!f.dispatcher->stateOf(Id::Nr3, kSliceA).on);
        QCOMPARE(f.b->activeNr(), NrSlot::Off);
        // A refusal once does not stick to the next click.
        QCOMPARE(f.dispatcher->click(Id::Nr4, kSliceA), QString());
        QCOMPARE(f.a->activeNr(), NrSlot::NR4);
    }

    // Acceptance: steps clamp at the ends.
    void stepsClampAtTheEnds()
    {
        Fixture f;
        f.a->setAfGain(ControlRanges::kAfGainMax - 2);
        f.dispatcher->click(Id::AfP5, kSliceA);
        QCOMPARE(f.a->afGain(), ControlRanges::kAfGainMax);
        f.dispatcher->click(Id::AfP5, kSliceA);
        QCOMPARE(f.a->afGain(), ControlRanges::kAfGainMax);
        f.a->setAfGain(ControlRanges::kAfGainMin + 3);
        f.dispatcher->click(Id::AfM5, kSliceA);
        QCOMPARE(f.a->afGain(), ControlRanges::kAfGainMin);
        f.dispatcher->click(Id::AfM5, kSliceA);
        QCOMPARE(f.a->afGain(), ControlRanges::kAfGainMin);

        f.a->setAudioPan(0.96);
        f.dispatcher->click(Id::BalP5, kSliceA);
        QCOMPARE(f.a->audioPan(), 1.0);
        f.dispatcher->click(Id::BalP5, kSliceA);
        QCOMPARE(f.a->audioPan(), 1.0);
        f.a->setAudioPan(-0.96);
        f.dispatcher->click(Id::BalM5, kSliceA);
        QCOMPARE(f.a->audioPan(), -1.0);
        f.dispatcher->click(Id::BalM5, kSliceA);
        QCOMPARE(f.a->audioPan(), -1.0);

        f.a->setAgcThreshold(ControlRanges::kAgcThresholdMaxDb - 1);
        f.dispatcher->click(Id::AgcP5, kSliceA);
        QCOMPARE(f.a->agcThreshold(), ControlRanges::kAgcThresholdMaxDb);
        f.dispatcher->click(Id::AgcP5, kSliceA);
        QCOMPARE(f.a->agcThreshold(), ControlRanges::kAgcThresholdMaxDb);
        f.a->setAgcThreshold(ControlRanges::kAgcThresholdMinDb + 4);
        f.dispatcher->click(Id::AgcM5, kSliceA);
        QCOMPARE(f.a->agcThreshold(), ControlRanges::kAgcThresholdMinDb);
        f.dispatcher->click(Id::AgcM5, kSliceA);
        QCOMPARE(f.a->agcThreshold(), ControlRanges::kAgcThresholdMinDb);
    }

    // Acceptance: a container on a slice that is not open shows every row
    // unavailable with noSliceReason, and a click changes nothing.
    void aSliceNotOpenShowsEveryRowUnavailable()
    {
        Fixture f;
        const SliceSnapshot a = SliceSnapshot::of(*f.a);
        const SliceSnapshot b = SliceSnapshot::of(*f.b);
        const int notches = f.model.notchModel()->notches().size();
        const QString reason = ContainerButtonDispatcher::noSliceReason(kSliceC);
        for (Id id : kReceive) {
            const auto st = f.dispatcher->stateOf(id, kSliceC);
            QVERIFY2(!st.available, qPrintable(QString::number(int(id))));
            QCOMPARE(st.reason, reason);
            QVERIFY(!st.on);
            QCOMPARE(f.dispatcher->click(id, kSliceC), reason);
        }
        QVERIFY(SliceSnapshot::of(*f.a) == a);
        QVERIFY(SliceSnapshot::of(*f.b) == b);
        QCOMPARE(f.model.notchModel()->notches().size(), notches);

        OtherButtonItem item;
        f.dispatcher->apply(&item, kSliceC);
        for (Id id : kReceive) {
            QVERIFY(!item.isButtonAvailable(id));
        }
    }

    // Acceptance: Mute All skips slices this window may not change.
    void muteAllSkipsSlicesThisWindowMayNotChange()
    {
        {
            Fixture f;
            QVERIFY(!f.dispatcher->stateOf(Id::MuteAll, kSliceA).on);
            f.dispatcher->click(Id::MuteAll, kSliceA);
            QVERIFY(f.a->muted());
            QVERIFY(f.b->muted());
            QVERIFY(f.dispatcher->stateOf(Id::MuteAll, kSliceA).on);
            // One unmuted: not all muted, so the next click mutes all.
            f.b->setMuted(false);
            QVERIFY(!f.dispatcher->stateOf(Id::MuteAll, kSliceA).on);
            f.dispatcher->click(Id::MuteAll, kSliceA);
            QVERIFY(f.a->muted() && f.b->muted());
            f.dispatcher->click(Id::MuteAll, kSliceA);
            QVERIFY(!f.a->muted() && !f.b->muted());
        }
        {
            Fixture f;
            f.refused.insert(1);  // slice B is another device's
            f.dispatcher->click(Id::MuteAll, kSliceA);
            QVERIFY(f.a->muted());
            QVERIFY(!f.b->muted());
            // Lit: every slice this window may change is muted.
            QVERIFY(f.dispatcher->stateOf(Id::MuteAll, kSliceA).on);
            f.dispatcher->click(Id::MuteAll, kSliceA);
            QVERIFY(!f.a->muted());
            QVERIFY(!f.b->muted());
        }
    }

    // MNF+ adds a notch on the container's slice (slice A's frequency, not
    // the active slice B's).
    void mnfPlusAddsANotchOnTheContainersSlice()
    {
        Fixture f;
        NotchModel* notches = f.model.notchModel();
        QVERIFY(notches);
        const int before = notches->notches().size();
        QCOMPARE(f.dispatcher->click(Id::MnfPlus, kSliceA), QString());
        QCOMPARE(notches->notches().size(), before + 1);
        const double centre = notches->notches().last().centerHz;
        QCOMPARE(centre, RadioModel::tnfCentreHzFor(*f.a));
        QVERIFY(qAbs(centre - f.a->frequency()) < 10000.0);
        QVERIFY(!f.dispatcher->stateOf(Id::MnfPlus, kSliceA).on);
    }

    // MNF+ is refused while transmitting, as the filter display's +MNF is.
    void mnfPlusIsRefusedWhileTransmitting()
    {
        // A remote window holds the Core's transmit state as observed; a
        // local model's only comes from keying, which this test never does.
        RadioModel remote(RadioModel::Role::Remote);
        QVERIFY(remote.addSliceWithStationId(0, QString()) >= 0);
        SliceModel* slice = remote.sliceById(0);
        QVERIFY(slice);
        QVERIFY(remote.notchModel());
        ContainerButtonDispatcher::Hooks hooks;
        ContainerButtonDispatcher dispatcher(&remote, std::move(hooks));
        QVERIFY(dispatcher.stateOf(Id::MnfPlus, kSliceA).available);
        QVERIFY(remote.applyMirroredValue("transmitting", true).isEmpty());
        QVERIFY(remote.isTransmitting());
        const auto st = dispatcher.stateOf(Id::MnfPlus, kSliceA);
        QVERIFY(!st.available);
        QCOMPARE(st.reason, ContainerButtonDispatcher::notchWhileTransmittingReason());
        const int before = remote.notchModel()->notches().size();
        QCOMPARE(dispatcher.click(Id::MnfPlus, kSliceA),
                 ContainerButtonDispatcher::notchWhileTransmittingReason());
        QCOMPARE(remote.notchModel()->notches().size(), before);
    }

    // None keys: every receive button clicked, several times, leaves the
    // radio on receive.
    void noneKeys()
    {
        Fixture f;
        QSignalSpy transmitting(&f.model, &RadioModel::transmittingChanged);
        for (int round = 0; round < 3; ++round) {
            for (Id id : kReceive) { f.dispatcher->click(id, kSliceA); }
            // Task 61: and the transmit setting buttons, VOX included.
            for (Id id : kTransmitSettings) { f.dispatcher->click(id, kSliceA); }
            // Task 62: and the display and window buttons.
            for (Id id : kDisplayAndWindow) { f.dispatcher->click(id, kSliceA); }
        }
        QVERIFY(!f.model.isTransmitting());
        QVERIFY(!f.model.transmitModel().isTune());
        QVERIFY(!f.model.moxController() || !f.model.moxController()->isMox());
        QCOMPARE(transmitting.count(), 0);
        QVERIFY(!f.model.isConnected());
    }

    // Each receive button is offered and built, so the editor lists it
    // enabled with no change to the editor.
    void everyReceiveButtonIsOfferedAndItsEditorRowEnabled()
    {
        checkOfferedWithEnabledEditorRows(kReceive, {
            "NR", "NR1", "NR2", "NR3", "NR4", "NB", "NB1", "NB2", "MNF+", "RIT", "RIT0",
            "XIT", "XIT0", "Lock", "TS+", "TS-", "Mute All", "Voice SQL", "AF+", "AF-",
            "BAL+", "BAL-", "FIX", "LONG", "SLOW", "MED", "FAST", "CUST", "AUTO", "AGC+",
            "AGC-"});
    }

    // Task 61: each transmit setting button is offered under its Thetis
    // caption, and its editor row is enabled.
    void everyTransmitSettingButtonIsOfferedAndItsEditorRowEnabled()
    {
        checkOfferedWithEnabledEditorRows(kTransmitSettings, {
            "COMP", "VOX", "DEXP", "TX EQ", "CFC", "CFC EQ", "MIC+", "MIC-", "COMP+",
            "COMP-", "VOX+", "VOX-", "Leveler", "PhaRot", "DRI+", "DRI-", "TUN+", "TUN-",
            "DRI-0", "TUN-0"});
    }

    // Task 61 acceptance, local window: each row sets the transmit model as
    // its applet control does; the toggles light from the model and go off
    // again; the steps never light. Nothing keys.
    void everyTransmitSettingRowSetsTheModel()
    {
        for (const TxRow& row : txRows()) {
            Fixture f;
            TransmitModel& tx = f.model.transmitModel();
            row.prepare(tx);
            const QByteArray name = QByteArray::number(int(row.id));
            const auto before = f.dispatcher->stateOf(row.id, kSliceA);
            QVERIFY2(before.available, name.constData());
            QVERIFY2(!before.on, name.constData());
            QCOMPARE(f.dispatcher->click(row.id, kSliceA), QString());
            QVERIFY2(row.changed(tx), name.constData());
            QCOMPARE(f.dispatcher->stateOf(row.id, kSliceA).on, row.litAfter);
            // A transmit setting is not the slice's: a container on any
            // slice shows the same, even one not open.
            QCOMPARE(f.dispatcher->stateOf(row.id, kSliceC).on, row.litAfter);
            QVERIFY(f.dispatcher->stateOf(row.id, kSliceC).available);
            if (row.litAfter) {
                QCOMPARE(f.dispatcher->click(row.id, kSliceA), QString());
                QVERIFY2(!f.dispatcher->stateOf(row.id, kSliceA).on, name.constData());
            }
            QVERIFY(!f.model.isTransmitting());
            QVERIFY(!tx.isTune());
            // apply() lights the grid from the model.
            OtherButtonItem item;
            row.prepare(tx);
            if (row.litAfter) { f.dispatcher->click(row.id, kSliceA); }
            f.dispatcher->apply(&item, kSliceA);
            QCOMPARE(item.buttonState(row.id), row.litAfter);
            QVERIFY(item.isButtonAvailable(row.id));
        }
    }

    // Task 61 acceptance: the steps clamp to their sliders' ranges: the
    // board's mic gain range, PROC 0 to 20 dB, VOX -80 to 0 dB, and RF and
    // Tune Power 0 to the radio's slider maximum (an HL2's 90 and 99).
    void transmitStepsClampToTheirSliders()
    {
        for (const bool hl2 : {false, true}) {
            Fixture f;
            if (hl2) { f.model.setHpsdrModelForTest(HPSDRModel::HERMESLITE); }
            const HPSDRModel radio = f.model.hardwareProfile().model;
            TransmitModel& tx = f.model.transmitModel();
            const BoardCapabilities& caps = f.model.boardCapabilities();
            const auto clicks = [&f](Id id, int times) {
                for (int i = 0; i < times; ++i) { f.dispatcher->click(id, kSliceA); }
            };

            tx.setMicGainDb(caps.micGainMaxDb - 1);
            clicks(Id::MicP1, 3);
            QCOMPARE(tx.micGainDb(), caps.micGainMaxDb);
            tx.setMicGainDb(caps.micGainMinDb + 1);
            clicks(Id::MicM1, 3);
            QCOMPARE(tx.micGainDb(), caps.micGainMinDb);

            tx.setCpdrLevelDb(19);
            clicks(Id::CompP1, 3);
            QCOMPARE(tx.cpdrLevelDb(), 20);
            tx.setCpdrLevelDb(1);
            clicks(Id::CompM1, 3);
            QCOMPARE(tx.cpdrLevelDb(), 0);

            tx.setVoxThresholdDb(-1);
            clicks(Id::VoxP1, 3);
            QCOMPARE(tx.voxThresholdDb(), 0);
            tx.setVoxThresholdDb(-79);
            clicks(Id::VoxM1, 3);
            QCOMPARE(tx.voxThresholdDb(), -80);

            const int rfMax = rfPowerSliderMaxFor(radio);
            QCOMPARE(rfMax, hl2 ? 90 : 100);
            tx.setPower(rfMax - 2);
            clicks(Id::DriveP5, 2);
            QCOMPARE(tx.power(), rfMax);
            tx.setPower(3);
            clicks(Id::DriveM5, 2);
            QCOMPARE(tx.power(), 0);

            const int tuneMax = tuneSliderMaxFor(radio);
            QCOMPARE(tuneMax, hl2 ? 99 : 100);
            setShownTunePower(tx, tuneMax - 2);
            clicks(Id::TuneP5, 2);
            QCOMPARE(shownTunePower(tx), tuneMax);
            setShownTunePower(tx, 3);
            clicks(Id::TuneM5, 2);
            QCOMPARE(shownTunePower(tx), 0);
            QVERIFY(!f.model.isTransmitting());
        }
    }

    // Task 61 acceptance, remote window with the gate refused: every row
    // is unavailable with its control's gate's reason, and a click changes
    // nothing. VOX shows its VOX line reason (no microphone line here).
    void aRefusedRemoteGateShowsItsReasonAndChangesNothing()
    {
        QuietUnconnectedTxFrequency quiet;
        RadioModel remote(RadioModel::Role::Remote);
        QVERIFY(remote.addSliceWithStationId(0, QString()) >= 0);
        ContainerButtonDispatcher::Hooks hooks;
        hooks.transmitSettingsRefusal = [](int version) {
            return QStringLiteral("Version %1 is refused.").arg(version);
        };
        hooks.voxRefusal = [] { return TxRefusals::micNotConnected().text; };
        ContainerButtonDispatcher dispatcher(&remote, std::move(hooks));
        TransmitModel& tx = remote.transmitModel();
        const TransmitSnapshot before = TransmitSnapshot::of(tx);
        for (Id id : kTransmitSettings) {
            const QString expected = id == Id::Vox ? TxRefusals::micNotConnected().text
                : (id == Id::CfcEq || id == Id::PhaseRot) ? QStringLiteral("Version 4 is refused.")
                : (id == Id::DriveP5 || id == Id::DriveM5 || id == Id::Drive0)
                    ? QStringLiteral("Version 1 is refused.")
                    : QStringLiteral("Version 2 is refused.");
            const auto st = dispatcher.stateOf(id, kSliceA);
            QVERIFY2(!st.available, qPrintable(QString::number(int(id))));
            QCOMPARE(st.reason, expected);
            QCOMPARE(dispatcher.click(id, kSliceA), expected);
        }
        QVERIFY(TransmitSnapshot::of(tx) == before);
        QVERIFY(!remote.isTransmitting());
        OtherButtonItem item;
        dispatcher.apply(&item, kSliceA);
        for (Id id : kTransmitSettings) { QVERIFY(!item.isButtonAvailable(id)); }

        // VOX with its line open still follows the chain gate after it.
        {
            ContainerButtonDispatcher::Hooks lineOpen;
            lineOpen.transmitSettingsRefusal = [](int version) {
                return QStringLiteral("Version %1 is refused.").arg(version);
            };
            lineOpen.voxRefusal = [] { return QString(); };
            ContainerButtonDispatcher d(&remote, std::move(lineOpen));
            QCOMPARE(d.stateOf(Id::Vox, kSliceA).reason, QStringLiteral("Version 2 is refused."));
        }
        // A remote window with no gate at all refuses with the plain reason.
        {
            ContainerButtonDispatcher d(&remote, ContainerButtonDispatcher::Hooks{});
            for (Id id : kTransmitSettings) {
                QCOMPARE(d.stateOf(id, kSliceA).reason,
                         IStationLink::transmitSettingsUnavailableReason());
                QCOMPARE(d.click(id, kSliceA), IStationLink::transmitSettingsUnavailableReason());
            }
        }
        QVERIFY(TransmitSnapshot::of(tx) == before);
    }

    // Task 61 acceptance: a remote TUN+ asks the Core, whose transmit
    // band's tune power rises by 5; TUN- and TUN-0 follow; nothing keys.
    void aRemoteTunePlusReachesTheCore()
    {
        QuietUnconnectedTxFrequency quiet;
        LoopbackSession s(m_securityDir.path(), this);
        TransmitModel& coreTx = s.core->transmitModel();
        SliceModel* const txSlice = s.core->txBoundSlice();
        QVERIFY(txSlice);
        txSlice->setFrequency(7'150'000.0);
        coreTx.setTunePowerForBand(Band::Band40m, 20);
        QTRY_COMPARE(coreTx.tunePowerForTxBand(), 20);
        QVERIFY(s.connect());
        TransmitModel& windowTx = s.window.transmitModel();
        QTRY_COMPARE(windowTx.tunePowerForTxBand(), 20);

        ContainerButtonDispatcher::Hooks hooks;
        hooks.transmitSettingsRefusal = [](int) { return QString(); };
        hooks.voxRefusal = [] { return QString(); };
        ContainerButtonDispatcher dispatcher(&s.window, std::move(hooks));
        QVERIFY(dispatcher.stateOf(Id::TuneP5, kSliceA).available);
        QCOMPARE(dispatcher.click(Id::TuneP5, kSliceA), QString());
        QTRY_COMPARE(coreTx.tunePowerForBand(Band::Band40m), 25);
        QCOMPARE(coreTx.tunePowerForTxBand(), 25);
        QTRY_COMPARE(windowTx.tunePowerForTxBand(), 25);
        QCOMPARE(dispatcher.click(Id::TuneM5, kSliceA), QString());
        QTRY_COMPARE(coreTx.tunePowerForBand(Band::Band40m), 20);
        QTRY_COMPARE(windowTx.tunePowerForTxBand(), 20);
        QCOMPARE(dispatcher.click(Id::Tun0, kSliceA), QString());
        QTRY_COMPARE(coreTx.tunePowerForBand(Band::Band40m), 0);
        QVERIFY(!s.core->isTransmitting());
        QVERIFY(!coreTx.isTune());
        QVERIFY(!s.core->moxController() || !s.core->moxController()->isMox());
        QVERIFY(!s.window.isTransmitting());
    }

    // A settings preview is inert: its click never reaches the window's
    // dispatcher, while the live grid's click does.
    void aPreviewClickNeverReachesTheDispatcher()
    {
        Fixture f;
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings{directory.filePath(QStringLiteral("actions.xml"))};
        ContainerWorkspaceStore store{settings};
        ContainerContentRegistry registry;
        QWidget dock;
        QSplitter splitter;
        ContainerManager manager{&dock, &splitter};
        manager.setWorkspaceAdapter(&store, &registry);
        MeterPoller poller;
        manager.setPreviewPoller(&poller);
        dock.resize(1200, 900);

        OtherButtonItem shape;
        shape.setOnlyVisible(Id::Nr);
        ContentEntry entry = registry.makeEntry(QStringLiteral("OTHERBTNS"));
        entry.id = QStringLiteral("receive-buttons");
        entry.config[QStringLiteral("legacyRecord")] = shape.serialize();
        ContainerDocument container;
        container.id = QStringLiteral("grid");
        container.name = QStringLiteral("grid");
        container.layout = ContentLayout::VerticalStack;
        container.geometry = QRect(10, 20, kSize, kSize);
        container.contents.append(entry);
        WorkspaceDocument document;
        document.containers.append(container);
        document.mainContainerId = container.id;
        QCOMPARE(manager.commitWorkspace(document, 0).status, CommitStatus::Saved);

        // MainWindow's wiring: a container's click goes to the dispatcher.
        int dispatched = 0;
        for (ContainerWidget* c : manager.allContainers()) {
            connect(c, &ContainerWidget::otherButtonClicked, this, [&f, &dispatched](int id) {
                ++dispatched;
                f.dispatcher->click(static_cast<Id>(id), kSliceA);
            });
        }
        ContainerContentHost* host = manager.contentHost(QStringLiteral("grid"));
        QVERIFY(host);
        QVERIFY(QTest::qWaitFor([&] { return firstGrid(host->meterSurfaces()) != nullptr; }, 5000));
        OtherButtonItem* live = firstGrid(host->meterSurfaces());
        QVERIFY(live->isButtonShown(Id::Nr));
        // MainWindow::watchContainerItems wires each live item through its
        // container.
        manager.container(QStringLiteral("grid"))->wireInteractiveItem(live);

        ContainerSettingsDialog dialog(manager.container(QStringLiteral("grid")), nullptr, &manager);
        dialog.refreshDraftView();
        auto* preview = dialog.findChild<ContainerPreviewWidget*>();
        QVERIFY(preview);
        OtherButtonItem* copy = firstGrid(preview->findChildren<MeterWidget*>());
        QVERIFY(copy);
        QVERIFY(copy != live);
        QVERIFY(copy->isButtonShown(Id::Nr));

        // The preview surface takes no mouse input at all, and even a click
        // the copy itself raises goes nowhere.
        for (MeterWidget* meter : preview->findChildren<MeterWidget*>()) {
            QVERIFY(meter->testAttribute(Qt::WA_TransparentForMouseEvents));
        }
        const TransmitSnapshot txBefore = TransmitSnapshot::of(f.model.transmitModel());
        const PanSnapshot panABefore = PanSnapshot::of(f.spectrumA);
        const PanSnapshot panBBefore = PanSnapshot::of(f.spectrumB);
        emit copy->otherButtonClicked(int(Id::Nr));
        for (Id id : kReceive) { emit copy->otherButtonClicked(int(id)); }
        // Task 61: and the transmit setting buttons.
        for (Id id : kTransmitSettings) { emit copy->otherButtonClicked(int(id)); }
        // Task 62: and the display and window buttons: no live pan
        // toggles and no window opens.
        for (Id id : kDisplayAndWindow) { emit copy->otherButtonClicked(int(id)); }
        QCOMPARE(dispatched, 0);
        QCOMPARE(f.a->activeNr(), NrSlot::Off);
        QVERIFY(TransmitSnapshot::of(f.model.transmitModel()) == txBefore);
        QVERIFY(PanSnapshot::of(f.spectrumA) == panABefore);
        QVERIFY(PanSnapshot::of(f.spectrumB) == panBBefore);
        QCOMPARE(f.setups + f.diversities + f.linearities, 0);
        QVERIFY(f.volumeSteps.isEmpty());
        QVERIFY(!AppSettings::instance().contains(QStringLiteral("IsSpotsEnabled")));

        // The live grid's click reaches it (the wiring above is real).
        emit live->otherButtonClicked(int(Id::Nr));
        QCOMPARE(dispatched, 1);
        QCOMPARE(f.a->activeNr(), NrSlot::NR1);
        dialog.reject();
    }

    // Task 62: each display and window button is offered under its Thetis
    // caption, and its editor row is enabled.
    void everyDisplayAndWindowButtonIsOfferedAndItsEditorRowEnabled()
    {
        checkOfferedWithEnabledEditorRows(kDisplayAndWindow, {
            "NF", "Peak Blobs", "Cur Info", "Spots", "Fill", "ZTB", "A-Peak", "MAF+", "MAF-",
            "Setup", "Diversity", "Linearity"});
    }

    // Task 62 acceptance: each toggle changes the panadapter of the
    // container's slice (A's, while slice B is active and has its own) and
    // lights from it at once; slice B's container lights from B's pan.
    void displayTogglesActOnTheContainerSlicesPanadapter()
    {
        for (const PanToggle& t : kPanToggles) {
            Fixture f;
            const QByteArray name = QByteArray::number(int(t.id));
            const bool a0 = (f.spectrumA.*t.get)();
            const bool b0 = (f.spectrumB.*t.get)();
            const auto before = f.dispatcher->stateOf(t.id, kSliceA);
            QVERIFY2(before.available, name.constData());
            QCOMPARE(before.on, a0);

            QCOMPARE(f.dispatcher->click(t.id, kSliceA), QString());
            QCOMPARE((f.spectrumA.*t.get)(), !a0);
            QCOMPARE((f.spectrumB.*t.get)(), b0);
            QCOMPARE(f.dispatcher->stateOf(t.id, kSliceA).on, !a0);
            QCOMPARE(f.dispatcher->stateOf(t.id, kSliceA + 1).on, b0);
            QCOMPARE(f.model.activeSlice(), f.b);
            // The pan's own value, as its pill and Setup read it.
            QCOMPARE(f.spectrumA.perPanDisplayValue(t.keyBase),
                     !a0 ? QStringLiteral("True") : QStringLiteral("False"));
            OtherButtonItem item;
            f.dispatcher->apply(&item, kSliceA);
            QCOMPARE(item.buttonState(t.id), !a0);
            QVERIFY(item.isButtonAvailable(t.id));

            QCOMPARE(f.dispatcher->click(t.id, kSliceA), QString());
            QCOMPARE((f.spectrumA.*t.get)(), a0);
            QCOMPARE((f.spectrumB.*t.get)(), b0);
        }
    }

    // Task 62: Peak Blobs and A-Peak are saved as the pan's own value (the
    // per-pan path Setup's Peaks page takes for a selected pan), never as
    // the Setup default every pan inherits.
    void peakBlobsAndActivePeakSaveThePansOwnValue()
    {
        for (const QString& key : {QStringLiteral("DisplayPeakBlobsEnabled"),
                                   QStringLiteral("DisplayActivePeakHoldEnabled")}) {
            Fixture f;
            const Id id = key == QStringLiteral("DisplayPeakBlobsEnabled") ? Id::PeakBlobs
                                                                           : Id::ActivePeak;
            QCOMPARE(f.dispatcher->click(id, kSliceA), QString());
            QTRY_VERIFY2(PanDisplayKeys::hasOverride(key, f.spectrumA.panIndex()),
                         qPrintable(key));
            QVERIFY(!AppSettings::instance().contains(PanDisplayKeys::defaultKey(key)));
        }
    }

    // Task 62: ZTB zooms the container slice's pan only, and never lights.
    void ztbZoomsTheContainerSlicesPanadapter()
    {
        BandPlanManager plans;
        plans.loadPlans();
        QVERIFY(!plans.segments().isEmpty());
        Fixture f;
        for (SpectrumWidget* w : {&f.spectrumA, &f.spectrumB}) {
            w->setBandPlanManager(&plans);
            w->setVfoFrequency(14'100'000.0);
            w->setFrequencyRange(14'100'000.0, 48'000.0);
        }
        QSignalSpy zoomA(&f.spectrumA, &SpectrumWidget::bandwidthChangeRequested);
        QSignalSpy zoomB(&f.spectrumB, &SpectrumWidget::bandwidthChangeRequested);
        QVERIFY(f.dispatcher->stateOf(Id::Ztb, kSliceA).available);
        QCOMPARE(f.dispatcher->click(Id::Ztb, kSliceA), QString());
        QCOMPARE(zoomA.count(), 1);
        QCOMPARE(zoomB.count(), 0);
        QVERIFY(!f.dispatcher->stateOf(Id::Ztb, kSliceA).on);
        for (SpectrumWidget* w : {&f.spectrumA, &f.spectrumB}) { w->setBandPlanManager(nullptr); }
    }

    // Task 62 acceptance: with no panadapter showing the slice, the pan
    // buttons say so and a click changes nothing; a slice that is not open
    // says that instead. Spots, MAF and the windows are not the slice's.
    void withNoPanadapterThePanButtonsSaySo()
    {
        Fixture f;
        f.noPan.insert(0);  // slice A
        const PanSnapshot a = PanSnapshot::of(f.spectrumA);
        const PanSnapshot b = PanSnapshot::of(f.spectrumB);
        for (Id id : {Id::Nf, Id::PeakBlobs, Id::CursorInfo, Id::FillSpectrum, Id::Ztb,
                      Id::ActivePeak}) {
            const auto st = f.dispatcher->stateOf(id, kSliceA);
            QVERIFY2(!st.available, qPrintable(QString::number(int(id))));
            QCOMPARE(st.reason, QStringLiteral("No panadapter shows this slice."));
            QCOMPARE(st.reason, ContainerButtonDispatcher::noPanadapterReason());
            QCOMPARE(f.dispatcher->click(id, kSliceA), st.reason);
            QCOMPARE(f.dispatcher->stateOf(id, kSliceC).reason,
                     ContainerButtonDispatcher::noSliceReason(kSliceC));
        }
        QVERIFY(PanSnapshot::of(f.spectrumA) == a);
        QVERIFY(PanSnapshot::of(f.spectrumB) == b);
        for (Id id : {Id::Spots, Id::MafP5, Id::MafM5, Id::FormSetup, Id::FormDiversity,
                      Id::FormLinearity}) {
            QVERIFY2(f.dispatcher->stateOf(id, kSliceA).available,
                     qPrintable(QString::number(int(id))));
            QVERIFY(f.dispatcher->stateOf(id, kSliceC).available);
        }
        // Peak keeps the same words.
        QCOMPARE(f.dispatcher->stateOf(Id::PeakHold, kSliceA).reason,
                 ContainerButtonDispatcher::noPanadapterReason());
    }

    // Task 62 acceptance: Spots is saved as the Spot Hub saves it, and
    // every pan follows, slice B's included.
    void spotsPersistAndEveryPanFollows()
    {
        Fixture f;
        QVERIFY(f.spectrumA.showSpots());
        QVERIFY(f.spectrumB.showSpots());
        QVERIFY(f.dispatcher->stateOf(Id::Spots, kSliceA).on);
        QCOMPARE(f.dispatcher->click(Id::Spots, kSliceA), QString());
        QCOMPARE(AppSettings::instance().value(QStringLiteral("IsSpotsEnabled")).toString(),
                 QStringLiteral("False"));
        QVERIFY(!f.spectrumA.showSpots());
        QVERIFY(!f.spectrumB.showSpots());
        QVERIFY(!f.dispatcher->stateOf(Id::Spots, kSliceA).on);
        // A container on any slice shows the same.
        QVERIFY(!f.dispatcher->stateOf(Id::Spots, kSliceC).on);
        // A pan that reads its spot settings later (a new pan) follows.
        SpectrumWidget later;
        later.loadSpotDisplaySettings();
        QVERIFY(!later.showSpots());
        QCOMPARE(f.dispatcher->click(Id::Spots, kSliceC), QString());
        QCOMPARE(AppSettings::instance().value(QStringLiteral("IsSpotsEnabled")).toString(),
                 QStringLiteral("True"));
        QVERIFY(f.spectrumA.showSpots());
        QVERIFY(f.spectrumB.showSpots());
        QVERIFY(f.dispatcher->stateOf(Id::Spots, kSliceA).on);
    }

    // Task 62 acceptance: MAF+ and MAF- move the master volume by 5 on the
    // slider (0.05), saved and announced as the slider does, clamped to
    // 0..1; they never light.
    void mafStepsTheMasterVolumeLikeItsSlider()
    {
        AppSettings::instance().setValue(QStringLiteral("audio/Master/Volume"),
                                         QStringLiteral("0.500"));
        Fixture f;
        MasterOutputWidget widget(nullptr);
        f.master = &widget;
        auto* slider = widget.findChild<QSlider*>();
        QVERIFY(slider);
        QCOMPARE(slider->value(), 50);
        QSignalSpy changed(&widget, &MasterOutputWidget::volumeChanged);
        const auto saved = [] {
            return AppSettings::instance().value(QStringLiteral("audio/Master/Volume")).toString();
        };

        QCOMPARE(f.dispatcher->click(Id::MafP5, kSliceA), QString());
        QCOMPARE(slider->value(), 55);
        QCOMPARE(saved(), QStringLiteral("0.550"));
        QCOMPARE(changed.count(), 1);
        QCOMPARE(changed.last().first().toFloat(), 0.55f);
        QCOMPARE(f.dispatcher->click(Id::MafM5, kSliceA), QString());
        QCOMPARE(f.dispatcher->click(Id::MafM5, kSliceA), QString());
        QCOMPARE(slider->value(), 45);
        QCOMPARE(saved(), QStringLiteral("0.450"));
        QCOMPARE(f.volumeSteps, (QList<double>{0.05, -0.05, -0.05}));

        slider->setValue(97);
        f.dispatcher->click(Id::MafP5, kSliceA);
        QCOMPARE(slider->value(), 100);
        QCOMPARE(saved(), QStringLiteral("1.000"));
        const int atTop = changed.count();
        f.dispatcher->click(Id::MafP5, kSliceA);
        QCOMPARE(slider->value(), 100);
        QCOMPARE(changed.count(), atTop);
        slider->setValue(3);
        f.dispatcher->click(Id::MafM5, kSliceA);
        QCOMPARE(slider->value(), 0);
        QCOMPARE(saved(), QStringLiteral("0.000"));
        f.dispatcher->click(Id::MafM5, kSliceA);
        QCOMPARE(slider->value(), 0);
        QVERIFY(!f.dispatcher->stateOf(Id::MafP5, kSliceA).on);
        QVERIFY(!f.dispatcher->stateOf(Id::MafM5, kSliceA).on);
        f.master = nullptr;
    }

    // Task 62 acceptance: Setup, Diversity and Linearity open their
    // windows and never light; Linearity follows its menu entry's gate
    // (disabled with the entry's reason, never hidden); with no window to
    // open the button says it does nothing.
    void windowButtonsOpenTheirWindowsUnderTheirMenusRules()
    {
        Fixture f;
        QCOMPARE(f.dispatcher->click(Id::FormSetup, kSliceA), QString());
        QCOMPARE(f.dispatcher->click(Id::FormDiversity, kSliceA), QString());
        QCOMPARE(f.dispatcher->click(Id::FormLinearity, kSliceA), QString());
        QCOMPARE(f.setups, 1);
        QCOMPARE(f.diversities, 1);
        QCOMPARE(f.linearities, 1);
        for (Id id : {Id::FormSetup, Id::FormDiversity, Id::FormLinearity}) {
            QVERIFY(!f.dispatcher->stateOf(id, kSliceA).on);
        }

        f.linearityRefusal = QStringLiteral("The connected Core has not advertised PureSignal 3.");
        const auto st = f.dispatcher->stateOf(Id::FormLinearity, kSliceA);
        QVERIFY(!st.available);
        QCOMPARE(st.reason, f.linearityRefusal);
        QCOMPARE(f.dispatcher->click(Id::FormLinearity, kSliceA), f.linearityRefusal);
        QCOMPARE(f.linearities, 1);
        OtherButtonItem item;
        item.setButtonVisible(Id::FormLinearity, true);
        f.dispatcher->apply(&item, kSliceA);
        QVERIFY(!item.isButtonAvailable(Id::FormLinearity));
        QVERIFY(item.isButtonShown(Id::FormLinearity));
        // Setup and Diversity stay open to a click, as their menu entries.
        QVERIFY(f.dispatcher->stateOf(Id::FormSetup, kSliceA).available);
        QVERIFY(f.dispatcher->stateOf(Id::FormDiversity, kSliceA).available);

        // A window that supplies none of these hooks.
        ContainerButtonDispatcher bare(&f.model, ContainerButtonDispatcher::Hooks{});
        for (Id id : kDisplayAndWindow) {
            const auto none = bare.stateOf(id, kSliceA);
            QVERIFY2(!none.available, qPrintable(QString::number(int(id))));
            QVERIFY(!none.reason.isEmpty());
            QCOMPARE(bare.click(id, kSliceA), none.reason);
        }
    }

    // UI evidence: a grid of every receive button, lit from slice A.
    void captureTheReceiveGrid()
    {
        const QString dir = qEnvironmentVariable("NEREUS_OTHER_BUTTONS_CAPTURE_DIR");
        if (dir.isEmpty()) { QSKIP("Set NEREUS_OTHER_BUTTONS_CAPTURE_DIR to capture the grid."); }
        Fixture f;
        f.a->setActiveNr(NrSlot::NR2);
        f.a->setNbMode(NbMode::NB2);
        f.a->setRitEnabled(true);
        f.a->setSsqlEnabled(true);
        f.a->setAgcMode(AGCMode::Med);
        OtherButtonItem item;
        item.setVisibleGroups(QVector<qint32>(OtherButtonItem::kGroupCount, 0));
        for (Id id : kReceive) { item.setButtonVisible(id, true); }
        item.setRect(0.0f, 0.0f, 1.0f, 1.0f);
        f.dispatcher->apply(&item, kSliceA);
        QImage image(kSize, kSize, QImage::Format_ARGB32_Premultiplied);
        image.fill(QColor(0x0f, 0x0f, 0x1a));
        QPainter p(&image);
        item.paint(p, kSize, kSize);
        p.end();
        QVERIFY(QDir().mkpath(dir));
        QVERIFY(image.save(QDir(dir).filePath(QStringLiteral("receive-buttons-grid.png"))));
    }

    // Task 62 UI evidence: the display and window buttons, NF and Fill lit
    // from slice A's pan, Linearity disabled with its menu entry's reason.
    void captureTheDisplayAndWindowGrid()
    {
        const QString dir = qEnvironmentVariable("NEREUS_OTHER_BUTTONS_CAPTURE_DIR");
        if (dir.isEmpty()) { QSKIP("Set NEREUS_OTHER_BUTTONS_CAPTURE_DIR to capture the grid."); }
        Fixture f;
        f.spectrumA.setShowNoiseFloor(true);
        f.spectrumA.setPanFillEnabled(true);
        f.linearityRefusal = QStringLiteral("The connected Core has not advertised PureSignal 3.");
        OtherButtonItem item;
        item.setVisibleGroups(QVector<qint32>(OtherButtonItem::kGroupCount, 0));
        for (Id id : kDisplayAndWindow) { item.setButtonVisible(id, true); }
        item.setRect(0.0f, 0.0f, 1.0f, 1.0f);
        f.dispatcher->apply(&item, kSliceA);
        QImage image(kSize, kSize, QImage::Format_ARGB32_Premultiplied);
        image.fill(QColor(0x0f, 0x0f, 0x1a));
        QPainter p(&image);
        item.paint(p, kSize, kSize);
        p.end();
        QVERIFY(QDir().mkpath(dir));
        QVERIFY(image.save(QDir(dir).filePath(QStringLiteral("display-window-buttons-grid.png"))));
    }
};

QTEST_MAIN(TstOtherButtonActions)
#include "tst_other_button_actions.moc"
