// no-port-check: NereusSDR-original test file.
// =================================================================
// tests/tst_transmit_held_edits.cpp  (NereusSDR)
// =================================================================
//
// JJ's ruling, 2026-11-05: a transmit value set before any radio has connected
// stays, replaces the saved value of whichever radio connects next, and is
// saved for that radio. The dangerous half is a held write nobody made: at
// connect it would silently replace that radio's saved value, its drive
// included. So the real startups run here with no edits and must leave every
// saved value alone.
//
// Modification history (NereusSDR):
//   2026-11-05 - Original test for NereusSDR by J.J. Boyd (KG4VCF), with
//                 AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <QApplication>
#include <QMetaProperty>
#include <QSlider>

#include "core/AppSettings.h"
#include "core/CwTypes.h"
#include "core/TciProtocol.h"
#include "core/cat/CatService.h"
#include "core/SliceOwnership.h"
#include "core/MoxController.h"
#include "core/PaProfileManager.h"
#include "core/RadioConnection.h"
#include "core/TxSliceArbiter.h"
#include "core/daemon/DaemonApp.h"
#include "core/daemon/DaemonConfig.h"
#include "core/HpsdrModel.h"
#include "core/session/RemoteStationOptions.h"
#include "gui/MainWindow.h"
#include "gui/applets/TxApplet.h"
#include "gui/containers/ContainerButtonDispatcher.h"
#include "models/Band.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

namespace {

const QString kMac = QStringLiteral("aa:bb:cc:48:00:01");
const QString kOtherMac = QStringLiteral("aa:bb:cc:48:00:02");

QString radioKey(const QString& mac, const QString& relative)
{
    return QStringLiteral("hardware/%1/%2").arg(mac, relative);
}

QStringList heldKeys()
{
    QStringList held;
    for (const QString& key : AppSettings::instance().allKeys()) {
        if (key.startsWith(TransmitModel::heldEditsPrefix())) {
            held.append(key);
        }
    }
    return held;
}

QString saved(const QString& mac, const QString& relative)
{
    return AppSettings::instance().value(radioKey(mac, relative)).toString();
}

void connectLikeRadioModel(TransmitModel& model, const QString& mac)
{
    // RadioModel::connectToRadioImpl's order: load() reads tune power first.
    model.setMacAddress(mac);
    model.load();
    model.loadFromSettings(mac);
}

const QString kTune20m = QStringLiteral("tunePowerByBand/%1").arg(static_cast<int>(Band::Band20m));
const QString kTune40m = QStringLiteral("tunePowerByBand/%1").arg(static_cast<int>(Band::Band40m));

// Set with no radio and no transmit slice: no band yet.
QString heldPowerKey()
{
    return TransmitModel::heldEditsPrefix() + QStringLiteral("tx/PowerForNextBand");
}

QString heldTunePowerKey()
{
    return TransmitModel::heldEditsPrefix() + QStringLiteral("tx/TunePowerForNextBand");
}

void restartSettings()
{
    auto& s = AppSettings::instance();
    QVERIFY(s.save());
    s.clear();
    s.load();
}

// The starting slice comes back on the band it was closed on.
void saveStartingSliceOn40m()
{
    SliceModel saved;
    saved.setSliceIndex(0);
    saved.setFrequency(7'100'000.0);
    saved.setDspMode(DSPMode::LSB);
    saved.saveToSettings(Band::Band40m);
}

class DriveWatchConnection final : public RadioConnection {
    Q_OBJECT
public:
    QList<int> drives;
    int moxOnCalls{0};
    int iqCalls{0};

    explicit DriveWatchConnection(QObject* parent = nullptr) : RadioConnection(parent)
    {
        setState(ConnectionState::Connected);
    }

    void init() override {}
    void connectToRadio(const NereusSDR::RadioInfo&) override {}
    void disconnect() override {}
    void setReceiverFrequency(int, quint64) override {}
    void setTxFrequency(quint64) override {}
    void setActiveReceiverCount(int) override {}
    void setSampleRate(int) override {}
    void setAttenuator(int) override {}
    void setPreamp(bool) override {}
    void setTxDrive(int level) override { drives.append(level); }
    void sendTxIq(const float*, int) override { ++iqCalls; }
    void setWatchdogEnabled(bool) override {}
    void setAntennaRouting(AntennaRouting) override {}
    void setMox(bool on) override
    {
        if (on) {
            ++moxOnCalls;
        }
    }
    void setTrxRelay(bool) override {}
    void setMicBoost(bool) override {}
    void setLineIn(bool) override {}
    void setMicTipRing(bool) override {}
    void setMicBias(bool) override {}
    void setLineInGain(int) override {}
    void setUserDigOut(quint8) override {}
    void setPuresignalRun(bool) override {}
    void setMicPTTDisabled(bool) override {}
    void setMicXlr(bool) override {}
};

void settle()
{
    for (int i = 0; i < 3; ++i) {
        QCoreApplication::processEvents();
    }
}

// connectToRadioImpl's order, up to the radio answering. No radio has answered
// until radioAnswers, and a connect can end here.
void beginConnect(RadioModel& radio, const QString& mac)
{
    RadioInfo info;
    info.macAddress = mac;
    info.protocol = ProtocolVersion::Protocol2;
    radio.setLastRadioInfoForTest(info);
    radio.setHpsdrModelForTest(HPSDRModel::ANAN_G2);
    connectLikeRadioModel(radio.transmitModel(), mac);
    radio.applyConnectTransmitBandForTest();
    if (PaProfileManager* profiles = radio.paProfileManager()) {
        profiles->setMacAddress(mac);
        profiles->load(HPSDRModel::ANAN_G2);
    }
    const int start = radio.addStartingSlices(mac);
    if (SliceModel* slice = radio.sliceById(start)) {
        radio.setActiveSlice(static_cast<int>(radio.slices().indexOf(slice)));
        radio.loadSliceStateForTest(slice);
    }
    radio.configureStreamPool(2, 5, 192000);
    radio.bindUnboundSlices();
}

void radioAnswers(RadioModel& radio)
{
    radio.onConnectionStateChangedForTest(ConnectionState::Connected);
    settle();
}

// A window's queued step on Connected (MainWindow, Sub-Epic D Task 15): the
// saved layout's slices, then the saved transmit slice. It is queued by the
// connection state signal, before the rest of the Connected handling runs.
void addWindowsConnectStep(RadioModel& radio, const QStringList& pans)
{
    QObject::connect(&radio, &RadioModel::connectionStateChanged, &radio,
                     [&radio, pans](ConnectionState state) {
        if (state != ConnectionState::Connected) {
            return;
        }
        QMetaObject::invokeMethod(&radio, [&radio, pans] {
            MainWindow::populatePanSlices(&radio, pans, false, false);
            radio.txSliceArbiter()->finishRestore();
        }, Qt::QueuedConnection);
    });
}

// A radio with values of its own on everything the startup could disturb.
const QList<std::pair<QString, QString>>& radiosSavedValues()
{
    static const QList<std::pair<QString, QString>> values = {
        {QStringLiteral("tx/MicGain"), QStringLiteral("-11")},
        {QStringLiteral("tx/Mic_Source"), QStringLiteral("Radio")},
        {QStringLiteral("tx/FilterLow"), QStringLiteral("222")},
        {QStringLiteral("tx/FilterHigh"), QStringLiteral("2777")},
        {QStringLiteral("tx/Dexp_Threshold"), QStringLiteral("-33")},
        {QStringLiteral("tx/CW_KeyerSpeed"), QStringLiteral("31")},
        {QStringLiteral("tx/CompanderLevel"), QStringLiteral("7")},
        {QStringLiteral("tx/TXEQEnabled"), QStringLiteral("True")},
        {QStringLiteral("tx/TwoTonePower"), QStringLiteral("17")},
        {QStringLiteral("powerByBand/20m"), QStringLiteral("83")},
        {QStringLiteral("powerByBand/40m"), QStringLiteral("71")},
        {QStringLiteral("powerByBand/6m"), QStringLiteral("9")},
        {QStringLiteral("limitPowerByBand/20m"), QStringLiteral("91")},
        {QStringLiteral("limitTunePowerByBand/20m"), QStringLiteral("92")},
        {QStringLiteral("fmTxOffsetByBandMhz/20m"), QStringLiteral("1.25")},
        {kTune20m, QStringLiteral("23")},
    };
    return values;
}

void seedRadio(const QString& mac)
{
    for (const auto& [key, value] : radiosSavedValues()) {
        AppSettings::instance().setValue(radioKey(mac, key), value);
    }
}

// Run an operator-free startup's connect, and require nothing it did to count.
void verifyRadioUntouched(TransmitModel& model, const QString& mac)
{
    QCOMPARE(heldKeys(), QStringList{});
    connectLikeRadioModel(model, mac);
    for (const auto& [key, value] : radiosSavedValues()) {
        QVERIFY2(saved(mac, key) == value,
                 qPrintable(QStringLiteral("%1 became %2").arg(key, saved(mac, key))));
    }
    QCOMPARE(model.powerForBand(Band::Band20m), 83);
    QCOMPARE(model.powerForBand(Band::Band40m), 71);
    QCOMPARE(model.tunePowerForBand(Band::Band20m), 23);
    QCOMPARE(model.micGainDb(), -11);
    QCOMPARE(heldKeys(), QStringList{});
}

} // namespace

class TstTransmitHeldEdits : public QObject {
    Q_OBJECT

private slots:
    void init()
    {
        auto& s = AppSettings::instance();
        for (const QString& key : s.allKeys()) {
            if (key.startsWith(TransmitModel::heldEditsPrefix())
                || key.startsWith(QStringLiteral("tx/preconnect/"))
                || key.startsWith(QStringLiteral("Slice"))
                || key.startsWith(QStringLiteral("hardware/aa:bb:cc:48:"), Qt::CaseInsensitive)) {
                s.remove(key);
            }
        }
        // A Core's start reads the file, and a connect or a restart wrote it.
        QVERIFY(s.save());
    }

    // ── The ruling ──────────────────────────────────────────────────────

    void anEditReplacesTheRadiosSavedValueAndIsSaved()
    {
        AppSettings::instance().setValue(radioKey(kMac, QStringLiteral("tx/MicGain")),
                                         QStringLiteral("-11"));
        TransmitModel model;
        model.setKeepsEditsWithoutRadio(true);
        model.setMicGainDb(-3);
        QCOMPARE(heldKeys(),
                 QStringList{TransmitModel::heldEditsPrefix() + QStringLiteral("tx/MicGain")});

        connectLikeRadioModel(model, kMac);

        QCOMPARE(model.micGainDb(), -3);
        QCOMPARE(saved(kMac, QStringLiteral("tx/MicGain")), QStringLiteral("-3"));
        QCOMPARE(heldKeys(), QStringList{});
    }

    void anEditSurvivesARestartBeforeConnect()
    {
        auto& s = AppSettings::instance();
        s.setValue(radioKey(kMac, QStringLiteral("tx/MicGain")), QStringLiteral("-11"));
        {
            TransmitModel before;
            before.setKeepsEditsWithoutRadio(true);
            before.setMicGainDb(-3);
            before.setPowerForBand(Band::Band20m, 33);
        }
        QVERIFY(s.save());
        s.clear();
        s.load();

        // The auto-reconnect can run before startup is done, and still delivers.
        TransmitModel after;
        QVERIFY(!after.keepsEditsWithoutRadio());
        connectLikeRadioModel(after, kMac);
        QCOMPARE(after.micGainDb(), -3);
        QCOMPARE(after.powerForBand(Band::Band20m), 33);
        QCOMPARE(heldKeys(), QStringList{});
    }

    void aSecondRadioDoesNotGetIt()
    {
        AppSettings::instance().setValue(radioKey(kOtherMac, QStringLiteral("tx/MicGain")),
                                         QStringLiteral("-20"));
        TransmitModel model;
        model.setKeepsEditsWithoutRadio(true);
        model.setMicGainDb(-3);
        connectLikeRadioModel(model, kMac);
        connectLikeRadioModel(model, kOtherMac);
        QCOMPARE(model.micGainDb(), -20);
        QCOMPARE(saved(kOtherMac, QStringLiteral("tx/MicGain")), QStringLiteral("-20"));
        QCOMPARE(saved(kMac, QStringLiteral("tx/MicGain")), QStringLiteral("-3"));

        // After a first connect a radio-off edit goes to the last radio.
        model.setMicGainDb(-4);
        QCOMPARE(saved(kOtherMac, QStringLiteral("tx/MicGain")), QStringLiteral("-4"));
        QCOMPARE(heldKeys(), QStringList{});
    }

    void perBandPowerAndTunePowerComeThrough()
    {
        seedRadio(kMac);
        TransmitModel model;
        model.setKeepsEditsWithoutRadio(true);
        model.setPowerForBand(Band::Band20m, 33);
        model.setTunePowerForBand(Band::Band20m, 44);
        model.setLimitPowerForBand(Band::Band20m, 55);
        model.setLimitTunePowerForBand(Band::Band20m, 56);
        model.setFmTxOffsetForBandMhz(Band::Band20m, 2.5);

        connectLikeRadioModel(model, kMac);

        QCOMPARE(model.powerForBand(Band::Band20m), 33);
        QCOMPARE(model.tunePowerForBand(Band::Band20m), 44);
        QCOMPARE(model.limitPowerForBand(Band::Band20m), 55);
        QCOMPARE(model.limitTunePowerForBand(Band::Band20m), 56);
        QCOMPARE(model.fmTxOffsetForBandMhz(Band::Band20m), 2.5);
        QCOMPARE(saved(kMac, QStringLiteral("powerByBand/20m")), QStringLiteral("33"));
        QCOMPARE(saved(kMac, kTune20m), QStringLiteral("44"));
        // The bands not edited keep the radio's values.
        QCOMPARE(model.powerForBand(Band::Band40m), 71);
        QCOMPARE(heldKeys(), QStringList{});
    }

    // setTunePowerForTxBand needs a transmit band; with none it refuses and
    // the callers fall back to setTunePowerForBand with their own band.
    void tunePowerWithNoTransmitBandIsRefusedAndHoldsNothing()
    {
        TransmitModel model;
        model.setKeepsEditsWithoutRadio(true);
        QVERIFY(!model.setTunePowerForTxBand(44));
        QCOMPARE(heldKeys(), QStringList{});
        model.setTuneTxBand(Band::Band20m);
        QVERIFY(model.setTunePowerForTxBand(44));
        QCOMPARE(heldKeys(), QStringList{TransmitModel::heldEditsPrefix() + kTune20m});
    }

    void micSourceTakesTheHeldPath()
    {
        AppSettings::instance().setValue(radioKey(kMac, QStringLiteral("tx/Mic_Source")),
                                         QStringLiteral("Pc"));
        TransmitModel model;
        model.setKeepsEditsWithoutRadio(true);
        model.setMicSource(MicSource::Radio);
        QVERIFY(!AppSettings::instance().contains(QStringLiteral("tx/preconnect/Mic_Source")));
        QVERIFY(!AppSettings::instance().contains(QStringLiteral("tx/preconnect/Mic_Source_PreVax")));

        connectLikeRadioModel(model, kMac);

        QCOMPARE(model.micSource(), MicSource::Radio);
        QCOMPARE(saved(kMac, QStringLiteral("tx/Mic_Source")), QStringLiteral("Radio"));
        QCOMPARE(saved(kMac, QStringLiteral("tx/Mic_Source_PreVax")), QStringLiteral("Radio"));
        QCOMPARE(heldKeys(), QStringList{});
    }

    // ── VOX, MON and mic mute never load ────────────────────────────────

    void voxMonAndMicMuteAreNotHeldAndVoxIsOffAtConnect()
    {
        TransmitModel model;
        model.setKeepsEditsWithoutRadio(true);
        model.setVoxEnabled(true);
        model.setMonEnabled(!model.monEnabled());
        model.setMicMute(!model.micMute());
        QCOMPARE(heldKeys(), QStringList{});

        connectLikeRadioModel(model, kMac);

        QVERIFY(!model.voxEnabled());
        QCOMPARE(heldKeys(), QStringList{});
    }

    // A reconnect keeps VOX as it does today; only the no-radio case changed.
    void aLaterConnectLeavesVoxAsItWas()
    {
        TransmitModel model;
        connectLikeRadioModel(model, kMac);
        model.setVoxEnabled(true);
        connectLikeRadioModel(model, kMac);
        QVERIFY(model.voxEnabled());
    }

    // ── Trap 1: old tx/preconnect keys ──────────────────────────────────

    void anOldPreconnectKeyDoesNotOverrideARadiosMicSource()
    {
        auto& s = AppSettings::instance();
        s.setValue(QStringLiteral("tx/preconnect/Mic_Source"), QStringLiteral("Radio"));
        s.setValue(QStringLiteral("tx/preconnect/Mic_Source_PreVax"), QStringLiteral("Radio"));
        s.setValue(radioKey(kMac, QStringLiteral("tx/Mic_Source")), QStringLiteral("Pc"));

        TransmitModel model;
        model.setKeepsEditsWithoutRadio(true);
        connectLikeRadioModel(model, kMac);

        QCOMPARE(model.micSource(), MicSource::Pc);
        QCOMPARE(saved(kMac, QStringLiteral("tx/Mic_Source")), QStringLiteral("Pc"));
        QCOMPARE(heldKeys(), QStringList{});
    }

    void anOldPreconnectKeyStillSuppliesARadioWithNoSavedSource()
    {
        AppSettings::instance().setValue(QStringLiteral("tx/preconnect/Mic_Source"),
                                         QStringLiteral("Radio"));
        TransmitModel model;
        connectLikeRadioModel(model, kMac);
        QCOMPARE(model.micSource(), MicSource::Radio);
    }

    // ── Trap 2: writes nobody made ──────────────────────────────────────

    void beforeStartupIsDoneNothingIsHeld()
    {
        seedRadio(kMac);
        TransmitModel model;
        model.setMicGainDb(-3);
        model.setPowerForBand(Band::Band20m, 33);
        model.setTunePowerForBand(Band::Band20m, 44);
        model.setMicSource(MicSource::Radio);
        verifyRadioUntouched(model, kMac);
    }

    // applyTransmitBand saves the old band's values on every band change,
    // and a window writes a value back when its widget settles.
    void writingAPropertyBackUnchangedHoldsNothing()
    {
        TransmitModel model;
        model.setKeepsEditsWithoutRadio(true);
        const QMetaObject* meta = model.metaObject();
        for (int i = meta->propertyOffset(); i < meta->propertyCount(); ++i) {
            const QMetaProperty property = meta->property(i);
            if (!property.isWritable()) {
                continue;
            }
            property.write(&model, property.read(&model));
            QVERIFY2(heldKeys().isEmpty(),
                     qPrintable(QStringLiteral("%1 held %2")
                                    .arg(QLatin1String(property.name()), heldKeys().join(u' '))));
        }
        for (const Band band : {Band::Band20m, Band::Band40m}) {
            model.setPowerForBand(band, model.powerForBand(band));
            model.setTunePowerForBand(band, model.tunePowerForBand(band));
            model.setLimitPowerForBand(band, model.limitPowerForBand(band));
            model.setLimitTunePowerForBand(band, model.limitTunePowerForBand(band));
            model.setFmTxOffsetForBandMhz(band, model.fmTxOffsetForBandMhz(band));
        }
        QCOMPARE(heldKeys(), QStringList{});
    }

    void aSliceChangingBandsWithNoRadioHoldsNothing()
    {
        seedRadio(kMac);
        RadioModel radio;
        radio.setBoardForTest(HPSDRHW::HermesLite);
        SliceModel* slice = radio.sliceById(radio.addSlice(QStringLiteral("pan-0")));
        QVERIFY(slice);
        slice->setFrequency(14'100'000.0);
        radio.keepTransmitEditsWithoutRadio();
        QVERIFY(radio.transmitModel().keepsEditsWithoutRadio());

        slice->setFrequency(7'100'000.0);
        slice->setFrequency(14'100'000.0);

        verifyRadioUntouched(radio.transmitModel(), kMac);
    }

    // The RADE snap moves the transmit filter; that follows the mode.
    void theRadeFilterSnapWithNoRadioHoldsNothing()
    {
        seedRadio(kMac);
        RadioModel radio;
        radio.setBoardForTest(HPSDRHW::HermesLite);
        radio.keepTransmitEditsWithoutRadio();

        radio.snapTransmitFilterForModeForTest(DSPMode::RADE_U);
        QCOMPARE(radio.transmitModel().filterLow(), 650);
        QCOMPARE(heldKeys(), QStringList{});
        radio.snapTransmitFilterForModeForTest(DSPMode::USB);

        verifyRadioUntouched(radio.transmitModel(), kMac);
    }

    // Leaving CW takes QSK off for the time being; that follows the mode.
    void leavingCwWithNoRadioHoldsNothing()
    {
        seedRadio(kMac);
        RadioModel radio;
        radio.setBoardForTest(HPSDRHW::HermesLite);
        SliceModel* slice = radio.sliceById(radio.addSlice(QStringLiteral("pan-0")));
        QVERIFY(slice);
        slice->setDspMode(DSPMode::CWU);
        TransmitModel& tx = radio.transmitModel();
        tx.setCwBreakInMode(static_cast<int>(CwBreakInMode::Qsk));
        radio.keepTransmitEditsWithoutRadio();

        slice->setDspMode(DSPMode::USB);
        QCOMPARE(tx.cwBreakInMode(), static_cast<int>(CwBreakInMode::Off));

        verifyRadioUntouched(tx, kMac);
    }


    // Holding is for the operator's own edits in the app. A logger that sends
    // drive when it connects, before the radio does, must not replace that
    // radio's saved value at connect.
    void aCatWriteWithNoRadioHoldsNothing()
    {
        seedRadio(kMac);
        RadioModel radio;
        radio.setBoardForTest(HPSDRHW::HermesLite);
        radio.addSlice(QStringLiteral("pan-0"));
        for (SliceModel* slice : radio.slices()) {
            radio.sliceOwnership()->setOwner(slice->sliceIndex(), SliceOwnership::stationDevice());
        }
        CatEndpointConfig config;
        config.binding.primarySliceId = 0;
        QVERIFY(radio.catService()->applyChannelConfig(1, config));
        radio.catService()->startConfigured();
        const quint64 id = radio.catService()->openSession(1, CatTransportKind::Tester);
        QVERIFY(id);
        radio.keepTransmitEditsWithoutRadio();

        radio.catService()->processFrame(id, "ZZPC077;");
        QCOMPARE(radio.transmitModel().power(), 77);
        radio.catService()->processFrame(id, "ZZMG+12;");
        QCOMPARE(radio.transmitModel().micGainDb(), 12);
        radio.catService()->processFrame(id, "ZZTO033;");
        QCOMPARE(heldKeys(), QStringList{});

        // And the operator's own edit right after still is held.
        radio.transmitModel().setMicGainDb(-2);
        QCOMPARE(heldKeys(),
                 QStringList{TransmitModel::heldEditsPrefix() + QStringLiteral("tx/MicGain")});
    }

    void aTciWriteWithNoRadioHoldsNothing()
    {
        seedRadio(kMac);
        RadioModel radio;
        radio.setBoardForTest(HPSDRHW::HermesLite);
        radio.addSlice(QStringLiteral("pan-0"));
        radio.keepTransmitEditsWithoutRadio();
        TciProtocol tci(&radio);
        const int speed = radio.transmitModel().cwKeyerSpeed() == 33 ? 34 : 33;
        tci.handleCommand(QStringLiteral("cw_keyer_speed:%1;").arg(speed));
        QCOMPARE(radio.transmitModel().cwKeyerSpeed(), speed);
        tci.handleCommand(QStringLiteral("cw_macros_speed:29;"));
        tci.handleCommand(QStringLiteral("mon_volume:-20;"));
        QCOMPARE(heldKeys(), QStringList{});
    }

    // ── No radio and no slice: no band yet ──────────────────────────────

    // Before any slice exists no band is the radio's: a save would land on the
    // default band and replace that band's power on a radio that then comes up
    // on another. So it waits with no band, and goes to the band the radio
    // comes up on. Only that band changes.
    void aPowerEditBeforeAnySliceGoesToTheBandTheRadioComesUpOn()
    {
        seedRadio(kMac);
        saveStartingSliceOn40m();
        {
            RadioModel before;
            before.keepTransmitEditsWithoutRadio();
            before.transmitModel().setPower(33);
            QCOMPARE(before.transmitModel().power(), 33);
            QCOMPARE(heldKeys(), QStringList{heldPowerKey()});
        }
        restartSettings();
        QCOMPARE(heldKeys(), QStringList{heldPowerKey()});

        RadioModel radio;
        DriveWatchConnection connection;
        radio.injectConnectionForTest(&connection);
        const auto detach = qScopeGuard([&radio] { radio.injectConnectionForTest(nullptr); });
        radio.setConnectionStateForTest(ConnectionState::Connecting);
        beginConnect(radio, kMac);
        // The radio has not answered: nothing has moved.
        QCOMPARE(heldKeys(), QStringList{heldPowerKey()});
        QCOMPARE(radio.transmitModel().powerForBand(Band::Band40m), 71);
        connection.drives.clear();

        radioAnswers(radio);

        TransmitModel& tx = radio.transmitModel();
        QCOMPARE(radio.txBoundSlice(), radio.sliceById(0));
        QCOMPARE(bandFromFrequency(radio.txBoundSlice()->frequency()), Band::Band40m);
        QCOMPARE(heldKeys(), QStringList{});
        QCOMPARE(tx.power(), 33);
        QCOMPARE(tx.powerForBand(Band::Band40m), 33);
        QCOMPARE(saved(kMac, QStringLiteral("powerByBand/40m")), QStringLiteral("33"));
        QCOMPARE(tx.powerForBand(Band::Band20m), 83);
        QCOMPARE(saved(kMac, QStringLiteral("powerByBand/20m")), QStringLiteral("83"));
        QCOMPARE(tx.powerForBand(Band::Band6m), 9);
        QCOMPARE(saved(kMac, QStringLiteral("powerByBand/6m")), QStringLiteral("9"));

        // The drive is what the slider sets at that value.
        QVERIFY(!connection.drives.isEmpty());
        const int driveAtHeldValue = connection.drives.last();
        tx.setPower(71);
        settle();
        QVERIFY(connection.drives.last() != driveAtHeldValue);
        tx.setPower(33);
        settle();
        QCOMPARE(connection.drives.last(), driveAtHeldValue);

        // Connecting never keys.
        QCOMPARE(connection.moxOnCalls, 0);
        QCOMPARE(connection.iqCalls, 0);
        QVERIFY(!radio.moxController()->isMox());
    }

    // The Tune slider with no band: the applet's fallback path.
    void aTuneSliderEditBeforeAnySliceGoesToTheBandTheRadioComesUpOn()
    {
        seedRadio(kMac);
        saveStartingSliceOn40m();
        AppSettings::instance().setValue(radioKey(kMac, kTune40m), QStringLiteral("24"));
        {
            RadioModel before;
            TxApplet applet(&before);
            before.keepTransmitEditsWithoutRadio();
            QVERIFY(applet.tunePowerSlider());
            applet.tunePowerSlider()->setValue(44);
            QCOMPARE(applet.tunePowerSlider()->value(), 44);
            QCOMPARE(heldKeys(), QStringList{heldTunePowerKey()});
        }
        restartSettings();

        RadioModel radio;
        TxApplet applet(&radio);
        radio.setConnectionStateForTest(ConnectionState::Connecting);
        beginConnect(radio, kMac);
        QCOMPARE(heldKeys(), QStringList{heldTunePowerKey()});
        radioAnswers(radio);

        TransmitModel& tx = radio.transmitModel();
        QCOMPARE(heldKeys(), QStringList{});
        QCOMPARE(tx.tunePowerForBand(Band::Band40m), 44);
        QCOMPARE(tx.tunePowerForTxBand(), 44);
        QCOMPARE(applet.tunePowerSlider()->value(), 44);
        QCOMPARE(saved(kMac, kTune40m), QStringLiteral("44"));
        QCOMPARE(tx.tunePowerForBand(Band::Band20m), 23);
        QCOMPARE(saved(kMac, kTune20m), QStringLiteral("23"));
    }

    void aTuneButtonWithNoBandHoldsWithNoBandAndAddsUp()
    {
        RadioModel radio;
        radio.keepTransmitEditsWithoutRadio();
        ContainerButtonDispatcher::Hooks hooks;
        hooks.transmitPermitted = [] { return true; };
        ContainerButtonDispatcher dispatcher(&radio, std::move(hooks));
        const int start = radio.transmitModel().tunePowerForBand(Band::Band20m);

        using Id = ContainerButtonDispatcher::Id;
        QCOMPARE(dispatcher.click(Id::TuneM5, 0), QString());
        QCOMPARE(dispatcher.click(Id::TuneM5, 0), QString());

        QCOMPARE(heldKeys(), QStringList{heldTunePowerKey()});
        QCOMPARE(AppSettings::instance().value(heldTunePowerKey()).toInt(), start - 10);
        // Back where it started is still the operator's value.
        QCOMPARE(dispatcher.click(Id::TuneP5, 0), QString());
        QCOMPARE(dispatcher.click(Id::TuneP5, 0), QString());
        QCOMPARE(AppSettings::instance().value(heldTunePowerKey()).toInt(), start);
    }

    // After a PWR edit the tune power still has no band.
    void aPowerEditLeavesTunePowerWithNoBand()
    {
        RadioModel radio;
        radio.keepTransmitEditsWithoutRadio();
        radio.transmitModel().setPower(33);
        QVERIFY(!radio.transmitModel().tuneTxBandKnown());
        QVERIFY(!radio.transmitModel().setTunePowerForTxBand(44));
        QCOMPARE(heldKeys(), QStringList{heldPowerKey()});
    }

    // A connect that ends before the radio answers delivers nothing, and the
    // next one still does. Also when it ends with the step already queued.
    void aConnectThatDropsFirstKeepsItForTheNext()
    {
        seedRadio(kMac);
        saveStartingSliceOn40m();
        {
            RadioModel before;
            before.keepTransmitEditsWithoutRadio();
            before.transmitModel().setPower(33);
        }
        {
            RadioModel dropped;
            dropped.setConnectionStateForTest(ConnectionState::Connecting);
            beginConnect(dropped, kMac);
            dropped.setConnectionStateForTest(ConnectionState::Disconnected);
            settle();
        }
        QCOMPARE(heldKeys(), QStringList{heldPowerKey()});
        QCOMPARE(saved(kMac, QStringLiteral("powerByBand/40m")), QStringLiteral("71"));
        {
            RadioModel dropped;
            dropped.setConnectionStateForTest(ConnectionState::Connecting);
            beginConnect(dropped, kMac);
            dropped.onConnectionStateChangedForTest(ConnectionState::Connected);
            dropped.setConnectionStateForTest(ConnectionState::Disconnected);
            settle();
        }
        QCOMPARE(heldKeys(), QStringList{heldPowerKey()});
        QCOMPARE(saved(kMac, QStringLiteral("powerByBand/40m")), QStringLiteral("71"));

        RadioModel radio;
        radio.setConnectionStateForTest(ConnectionState::Connecting);
        beginConnect(radio, kMac);
        radioAnswers(radio);
        QCOMPARE(heldKeys(), QStringList{});
        QCOMPARE(saved(kMac, QStringLiteral("powerByBand/40m")), QStringLiteral("33"));
        QCOMPARE(radio.transmitModel().power(), 33);
    }

    // The transmit slice a window restores on Connected decides the band, not
    // the starting slice it had before then.
    void aHeldPowerWaitsForTheRestoredTransmitSlice()
    {
        seedRadio(kMac);
        const QStringList pans{QStringLiteral("pan-0"), QStringLiteral("pan-1")};
        {
            RadioModel before;
            before.setConnectionStateForTest(ConnectionState::Connecting);
            addWindowsConnectStep(before, pans);
            beginConnect(before, kMac);
            radioAnswers(before);
            SliceModel* a = before.sliceById(0);
            SliceModel* b = before.sliceById(1);
            QVERIFY(a);
            QVERIFY(b);
            a->setFrequency(14'100'000.0);
            b->setFrequency(7'100'000.0);
            QVERIFY(before.txSliceArbiter()->requestHandoff(1));
            before.flushPendingSettingsSave();
        }
        for (const auto& [key, value] : radiosSavedValues()) {
            AppSettings::instance().setValue(radioKey(kMac, key), value);
        }
        {
            RadioModel edit;
            edit.keepTransmitEditsWithoutRadio();
            edit.transmitModel().setPower(33);
        }

        RadioModel radio;
        radio.setConnectionStateForTest(ConnectionState::Connecting);
        addWindowsConnectStep(radio, pans);
        beginConnect(radio, kMac);
        radioAnswers(radio);

        QCOMPARE(radio.txBoundSlice(), radio.sliceById(1));
        QCOMPARE(heldKeys(), QStringList{});
        QCOMPARE(radio.transmitModel().powerForBand(Band::Band40m), 33);
        QCOMPARE(radio.transmitModel().powerForBand(Band::Band20m), 83);
        QCOMPARE(saved(kMac, QStringLiteral("powerByBand/20m")), QStringLiteral("83"));
        QCOMPARE(radio.transmitModel().power(), 33);
    }

    // A value held for a band keeps it. The band-less one is applied last, so
    // on the band the radio comes up on it wins, whichever came first.
    void aBandlessPowerAndAPageEditForTheSameBand()
    {
        seedRadio(kMac);
        saveStartingSliceOn40m();
        {
            RadioModel before;
            before.keepTransmitEditsWithoutRadio();
            before.transmitModel().setPower(33);
            before.transmitModel().setPowerForBand(Band::Band40m, 55);
            before.transmitModel().setPowerForBand(Band::Band6m, 44);
        }
        RadioModel radio;
        radio.setConnectionStateForTest(ConnectionState::Connecting);
        beginConnect(radio, kMac);
        radioAnswers(radio);
        QCOMPARE(heldKeys(), QStringList{});
        QCOMPARE(radio.transmitModel().powerForBand(Band::Band40m), 33);
        QCOMPARE(saved(kMac, QStringLiteral("powerByBand/40m")), QStringLiteral("33"));
        QCOMPARE(radio.transmitModel().powerForBand(Band::Band6m), 44);
        QCOMPARE(saved(kMac, QStringLiteral("powerByBand/6m")), QStringLiteral("44"));
    }

    // Nothing here runs on the air: PWR and the drive stay, and it waits.
    void aHeldPowerDoesNotApplyWhileMoxIsUp()
    {
        seedRadio(kMac);
        saveStartingSliceOn40m();
        {
            RadioModel before;
            before.keepTransmitEditsWithoutRadio();
            before.transmitModel().setPower(33);
        }
        RadioModel radio;
        DriveWatchConnection connection;
        radio.injectConnectionForTest(&connection);
        const auto detach = qScopeGuard([&radio] { radio.injectConnectionForTest(nullptr); });
        radio.moxController()->setTimerIntervals(0, 0, 0, 0, 0, 0);
        radio.setConnectionStateForTest(ConnectionState::Connecting);
        beginConnect(radio, kMac);
        radio.moxController()->setMox(true);
        settle();
        QVERIFY(radio.moxController()->isMox());
        const int powerOnAir = radio.transmitModel().power();
        connection.drives.clear();

        radioAnswers(radio);

        QCOMPARE(heldKeys(), QStringList{heldPowerKey()});
        QCOMPARE(radio.transmitModel().power(), powerOnAir);
        QCOMPARE(saved(kMac, QStringLiteral("powerByBand/40m")), QStringLiteral("71"));
        radio.moxController()->setMox(false);
        settle();
    }

    // A logger's drive is not held, band or no band, and neither is a window's
    // mirror of the Core's.
    void aPausedPowerEditWithNoSliceHoldsNothing()
    {
        seedRadio(kMac);
        RadioModel radio;
        radio.keepTransmitEditsWithoutRadio();
        {
            const TransmitModel::HeldEditPause notAnEdit(radio.transmitModel());
            radio.transmitModel().setPower(33);
        }
        QCOMPARE(heldKeys(), QStringList{});
        connectLikeRadioModel(radio.transmitModel(), kMac);
        QCOMPARE(saved(kMac, QStringLiteral("powerByBand/20m")), QStringLiteral("83"));

        RadioModel window{RadioModel::Role::Remote};
        window.keepTransmitEditsWithoutRadio();
        window.transmitModel().setPower(34);
        QCOMPARE(heldKeys(), QStringList{});
    }

    // A Core's restored slice is a transmit band, and it holds for that band.
    void aCoreWithASliceHoldsForItsBand()
    {
        seedRadio(kMac);
        DaemonConfig config = DaemonConfig::defaults();
        config.remotePort = 0;
        config.statusPage = false;
        DaemonApp app;
        app.primeBoardForTest(HPSDRHW::HermesLite);
        QVERIFY(app.start(config));
        RadioModel* radio = app.radioModelForTest();
        QVERIFY(radio);
        QVERIFY(radio->txBoundSlice());
        const Band band = bandFromFrequency(radio->txBoundSlice()->frequency());
        radio->transmitModel().setPower(radio->transmitModel().power() == 33 ? 34 : 33);
        QCOMPARE(heldKeys().join(u' '),
                 TransmitModel::heldEditsPrefix()
                     + QStringLiteral("powerByBand/%1").arg(bandKeyName(band)));
        app.stop();
    }

    void aPowerEditThroughTheRadioModelLandsOnTheTransmitBand()
    {
        seedRadio(kMac);
        RadioModel radio;
        radio.setBoardForTest(HPSDRHW::HermesLite);
        const int id = radio.addSlice(QStringLiteral("pan-0"));
        SliceModel* slice = radio.sliceById(id);
        QVERIFY(slice);
        slice->setFrequency(14'100'000.0);
        radio.keepTransmitEditsWithoutRadio();
        radio.transmitModel().setPower(radio.transmitModel().power() == 33 ? 34 : 33);
        const int edited = radio.transmitModel().power();
        QCOMPARE(heldKeys(),
                 QStringList{TransmitModel::heldEditsPrefix() + QStringLiteral("powerByBand/20m")});

        connectLikeRadioModel(radio.transmitModel(), kMac);
        QCOMPARE(radio.transmitModel().powerForBand(Band::Band20m), edited);
        QCOMPARE(saved(kMac, QStringLiteral("powerByBand/20m")), QString::number(edited));
        QCOMPARE(radio.transmitModel().powerForBand(Band::Band40m), 71);
    }

    void aWindowsRealStartupLeavesTheRadiosValuesAlone()
    {
        seedRadio(kMac);
        MainWindow window({}, nullptr, MainWindow::ConnectionStartup::Deferred);
        window.show();
        RadioModel* radio = window.radioModel();
        QVERIFY(radio);
        QTRY_VERIFY(radio->transmitModel().keepsEditsWithoutRadio());
        QTest::qWait(300);
        verifyRadioUntouched(radio->transmitModel(), kMac);
    }

    void aWindowHeldEditReachesTheRadioAndClosingHoldsNothing()
    {
        seedRadio(kMac);
        {
            MainWindow window({}, nullptr, MainWindow::ConnectionStartup::Deferred);
            window.show();
            QTRY_VERIFY(window.radioModel()->transmitModel().keepsEditsWithoutRadio());
            window.radioModel()->transmitModel().setMicGainDb(-2);
            QCOMPARE(heldKeys().size(), 1);
        }
        QCOMPARE(heldKeys().size(), 1);
        TransmitModel next;
        connectLikeRadioModel(next, kMac);
        QCOMPARE(next.micGainDb(), -2);
        QCOMPARE(next.powerForBand(Band::Band20m), 83);
    }

    void aCoresRealStartupLeavesTheRadiosValuesAlone()
    {
        seedRadio(kMac);
        DaemonConfig config = DaemonConfig::defaults();
        config.remotePort = 0;
        config.statusPage = false;
        DaemonApp app;
        app.primeBoardForTest(HPSDRHW::HermesLite);
        QVERIFY(app.start(config));
        RadioModel* radio = app.radioModelForTest();
        QVERIFY(radio);
        QVERIFY(radio->transmitModel().keepsEditsWithoutRadio());
        QTest::qWait(300);
        verifyRadioUntouched(radio->transmitModel(), kMac);
        app.stop();
    }

    // ── Trap 3: a window on a Core ──────────────────────────────────────

    void aRemoteWindowsModelNeverHolds()
    {
        RadioModel window{RadioModel::Role::Remote};
        window.keepTransmitEditsWithoutRadio();
        TransmitModel& tx = window.transmitModel();
        QVERIFY(!tx.keepsEditsWithoutRadio());
        tx.setMicGainDb(-3);
        tx.setPowerForBand(Band::Band20m, 33);
        tx.setTunePowerForBand(Band::Band20m, 44);
        tx.setMicSource(MicSource::Pc);
        QCOMPARE(heldKeys(), QStringList{});
    }

    void aRemoteMainWindowNeverStartsHolding()
    {
        RemoteStationOptions remote;
        remote.url = QStringLiteral("ws://127.0.0.1:1");
        remote.token = QStringLiteral("t");
        MainWindow window(remote, nullptr, MainWindow::ConnectionStartup::Deferred);
        QCOMPARE(window.radioModel()->role(), RadioModel::Role::Remote);
        QTest::qWait(50);
        QVERIFY(!window.radioModel()->transmitModel().keepsEditsWithoutRadio());
        QCOMPARE(heldKeys(), QStringList{});
    }
};

QTEST_MAIN(TstTransmitHeldEdits)
#include "tst_transmit_held_edits.moc"
