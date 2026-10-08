// no-port-check: test-only, NereusSDR-original. No upstream logic is
// ported here.
//
// =================================================================
// The radio speaker's state on RadioModel (R-SPK-05, R-SPK-06 local half,
// R-SPK-07, R-SPK-11, R-SPK-12, R-SPK-15, D4, D11, D17; V-SW-2, V-SW-5
// local half).
// =================================================================
//
// RADIO level, RADIO mute and the amplifier choice are saved per radio
// under hardware/<mac>/RadioSpeaker/..., loaded on connect (seeded from
// the engine's master level when the radio has none saved), and forwarded
// to the AudioEngine and to the connection. The model also reports whether
// there is a radio speaker to set and whether the amplifier can be
// switched, and why not. The "CW or Tune" flag reaches the connection
// before Tune keys.
//
// Modification history (NereusSDR):
//   2026-10-06 - New test. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <QCoreApplication>
#include <QScopeGuard>
#include <QSignalSpy>

#include <memory>

#include "core/AppSettings.h"
#include "core/AudioEngine.h"
#include "core/HpsdrModel.h"
#include "core/MoxController.h"
#include "core/RadioConnection.h"
#include "core/RadioDiscovery.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

#include "fakes/ConnectableRadioModel.h"

using namespace NereusSDR;

namespace {

const QString kMacA = QStringLiteral("aa:bb:cc:00:00:0a");
const QString kMacB = QStringLiteral("aa:bb:cc:00:00:0b");

// Records the calls the radio speaker sends, in order, and stores them
// through the base so the getters read back what arrived.
class LoggingConnection : public RadioConnection {
    Q_OBJECT
public:
    QStringList log;
    int protocol{1};
    bool carriesAudio{true};

    explicit LoggingConnection(QObject* parent = nullptr)
        : RadioConnection(parent)
    {
        setState(ConnectionState::Connected);
    }

    int protocolVersion() const override { return protocol; }
    bool carriesRadioAudio() const noexcept override { return carriesAudio; }

    void setSpeakerAmplifierMode(int mode) override
    {
        log << QStringLiteral("mode:%1").arg(mode);
        RadioConnection::setSpeakerAmplifierMode(mode);
    }
    void setRadioSpeakerMuted(bool muted) override
    {
        log << QStringLiteral("muted:%1").arg(int(muted));
        RadioConnection::setRadioSpeakerMuted(muted);
    }
    void setSidetoneExpected(bool expected) override
    {
        log << QStringLiteral("sidetone:%1").arg(int(expected));
        RadioConnection::setSidetoneExpected(expected);
    }
    void setMox(bool enabled) override
    {
        log << QStringLiteral("mox:%1").arg(int(enabled));
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
    void setTxDrive(int) override {}
    void sendTxIq(const float*, int) override {}
    void setWatchdogEnabled(bool) override {}
    void setAntennaRouting(AntennaRouting) override {}
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

RadioInfo radioInfo(const QString& mac,
                    ProtocolVersion protocol = ProtocolVersion::Protocol1)
{
    RadioInfo info;
    info.macAddress = mac;
    info.protocol = protocol;
    return info;
}

QVariant key(const QString& mac, const char* name)
{
    return AppSettings::instance().hardwareValue(
        mac, QStringLiteral("RadioSpeaker/") + QLatin1String(name));
}

void pump()
{
    for (int i = 0; i < 6; ++i) {
        QCoreApplication::processEvents();
    }
}

bool hasAmplifier(HPSDRModel m)
{
    // R-SPK-08, written out independently of HardwareProfile.
    switch (m) {
    case HPSDRModel::ANAN7000D:
    case HPSDRModel::ANAN8000D:
    case HPSDRModel::ANVELINAPRO3:
    case HPSDRModel::ANAN_G2:
    case HPSDRModel::ANAN_G2_1K:
    case HPSDRModel::REDPITAYA:
        return true;
    default:
        return false;
    }
}

} // namespace

class TestRadioSpeakerModel : public QObject {
    Q_OBJECT

private slots:
    void init() { AppSettings::instance().clear(); }
    void cleanup() { AppSettings::instance().clear(); }

    // ── V-SW-2: the upgrade seed ────────────────────────────────────────
    void connect_noSavedKeys_seedsFromMasterLevel()
    {
        RadioModel model;
        model.audioEngine()->setVolume(0.720f);
        LoggingConnection conn;
        model.setLastRadioInfoForTest(radioInfo(kMacA));
        model.injectConnectionForTest(&conn);
        const auto detach = qScopeGuard([&model] { model.injectConnectionForTest(nullptr); });
        model.loadRadioSpeakerForConnectForTest();

        QCOMPARE(model.radioSpeakerVolume(), 72);
        QCOMPARE(model.radioSpeakerMuted(), false);
        QCOMPARE(model.speakerAmplifierMode(), 0);
        QVERIFY(qAbs(model.audioEngine()->radioSpeakerVolume() - 0.72f) < 1e-4f);
        QCOMPARE(model.audioEngine()->radioSpeakerMuted(), false);
        QCOMPARE(conn.speakerAmplifierMode(), 0);
        QCOMPARE(conn.radioSpeakerMuted(), false);
        // Nothing is written until a value changes.
        QVERIFY(!key(kMacA, "Volume").isValid());
        QVERIFY(!key(kMacA, "Muted").isValid());
        QVERIFY(!key(kMacA, "AmplifierMode").isValid());
        model.injectConnectionForTest(nullptr);
    }

    void connect_headlessCore_seedsAtEngineDefault()
    {
        RadioModel model;  // nothing sets the master level
        LoggingConnection conn;
        model.setLastRadioInfoForTest(radioInfo(kMacA));
        model.injectConnectionForTest(&conn);
        const auto detach = qScopeGuard([&model] { model.injectConnectionForTest(nullptr); });
        model.loadRadioSpeakerForConnectForTest();
        QCOMPARE(model.radioSpeakerVolume(), 50);
        QVERIFY(!key(kMacA, "Volume").isValid());
        model.injectConnectionForTest(nullptr);
    }

    void connect_savedKeys_load()
    {
        AppSettings& s = AppSettings::instance();
        s.setHardwareValue(kMacA, QStringLiteral("RadioSpeaker/Volume"), 55);
        s.setHardwareValue(kMacA, QStringLiteral("RadioSpeaker/Muted"),
                           QStringLiteral("True"));
        s.setHardwareValue(kMacA, QStringLiteral("RadioSpeaker/AmplifierMode"), 1);

        RadioModel model;
        model.audioEngine()->setVolume(0.720f);
        LoggingConnection conn;
        model.setLastRadioInfoForTest(radioInfo(kMacA));
        model.injectConnectionForTest(&conn);
        const auto detach = qScopeGuard([&model] { model.injectConnectionForTest(nullptr); });
        QSignalSpy vol(&model, &RadioModel::radioSpeakerVolumeChanged);
        QSignalSpy mute(&model, &RadioModel::radioSpeakerMutedChanged);
        QSignalSpy mode(&model, &RadioModel::speakerAmplifierModeChanged);
        model.loadRadioSpeakerForConnectForTest();

        QCOMPARE(model.radioSpeakerVolume(), 55);
        QCOMPARE(model.radioSpeakerMuted(), true);
        QCOMPARE(model.speakerAmplifierMode(), 1);
        QCOMPARE(vol.count(), 1);
        QCOMPARE(mute.count(), 1);
        QCOMPARE(mode.count(), 1);
        QVERIFY(qAbs(model.audioEngine()->radioSpeakerVolume() - 0.55f) < 1e-4f);
        QCOMPARE(model.audioEngine()->radioSpeakerMuted(), true);
        QCOMPARE(conn.speakerAmplifierMode(), 1);
        QCOMPARE(conn.radioSpeakerMuted(), true);
        model.injectConnectionForTest(nullptr);
    }

    void twoRadios_keepTheirOwnValues()
    {
        RadioModel model;
        model.audioEngine()->setVolume(0.30f);
        LoggingConnection conn;
        model.injectConnectionForTest(&conn);
        const auto detach = qScopeGuard([&model] { model.injectConnectionForTest(nullptr); });

        model.setLastRadioInfoForTest(radioInfo(kMacA));
        model.loadRadioSpeakerForConnectForTest();
        model.setRadioSpeakerVolume(40);
        model.setSpeakerAmplifierMode(2);

        model.setLastRadioInfoForTest(radioInfo(kMacB));
        model.loadRadioSpeakerForConnectForTest();
        QCOMPARE(model.radioSpeakerVolume(), 30);  // B has none: seeded
        QCOMPARE(model.speakerAmplifierMode(), 0);
        model.setRadioSpeakerVolume(60);
        model.setRadioSpeakerMuted(true);

        model.setLastRadioInfoForTest(radioInfo(kMacA));
        model.loadRadioSpeakerForConnectForTest();
        QCOMPARE(model.radioSpeakerVolume(), 40);
        QCOMPARE(model.radioSpeakerMuted(), false);
        QCOMPARE(model.speakerAmplifierMode(), 2);
        QCOMPARE(conn.speakerAmplifierMode(), 2);

        model.setLastRadioInfoForTest(radioInfo(kMacB));
        model.loadRadioSpeakerForConnectForTest();
        QCOMPARE(model.radioSpeakerVolume(), 60);
        QCOMPARE(model.radioSpeakerMuted(), true);
        QCOMPARE(model.speakerAmplifierMode(), 0);
        QCOMPARE(key(kMacA, "Volume").toInt(), 40);
        QCOMPARE(key(kMacB, "Volume").toInt(), 60);
        model.injectConnectionForTest(nullptr);
    }

    // ── Setters: clamp, emit on change, save, forward ───────────────────
    void setters_clampEmitSaveAndForward()
    {
        RadioModel model;
        LoggingConnection conn;
        model.setLastRadioInfoForTest(radioInfo(kMacA));
        model.injectConnectionForTest(&conn);
        const auto detach = qScopeGuard([&model] { model.injectConnectionForTest(nullptr); });
        model.loadRadioSpeakerForConnectForTest();
        conn.log.clear();

        QSignalSpy vol(&model, &RadioModel::radioSpeakerVolumeChanged);
        QSignalSpy mute(&model, &RadioModel::radioSpeakerMutedChanged);
        QSignalSpy mode(&model, &RadioModel::speakerAmplifierModeChanged);

        model.setRadioSpeakerVolume(150);
        QCOMPARE(model.radioSpeakerVolume(), 100);
        model.setRadioSpeakerVolume(100);
        model.setRadioSpeakerVolume(-5);
        QCOMPARE(model.radioSpeakerVolume(), 0);
        QCOMPARE(vol.count(), 2);
        QCOMPARE(vol.at(0).at(0).toInt(), 100);
        QCOMPARE(vol.at(1).at(0).toInt(), 0);
        model.setRadioSpeakerVolume(64);
        QVERIFY(qAbs(model.audioEngine()->radioSpeakerVolume() - 0.64f) < 1e-4f);
        QCOMPARE(key(kMacA, "Volume").toInt(), 64);

        model.setRadioSpeakerMuted(true);
        model.setRadioSpeakerMuted(true);
        QCOMPARE(mute.count(), 1);
        QCOMPARE(model.audioEngine()->radioSpeakerMuted(), true);
        QCOMPARE(key(kMacA, "Muted").toString(), QStringLiteral("True"));
        model.setRadioSpeakerMuted(false);
        QCOMPARE(key(kMacA, "Muted").toString(), QStringLiteral("False"));

        model.setSpeakerAmplifierMode(5);
        QCOMPARE(model.speakerAmplifierMode(), 2);
        model.setSpeakerAmplifierMode(-1);
        QCOMPARE(model.speakerAmplifierMode(), 0);
        model.setSpeakerAmplifierMode(0);
        model.setSpeakerAmplifierMode(1);
        QCOMPARE(mode.count(), 3);
        QCOMPARE(key(kMacA, "AmplifierMode").toInt(), 1);

        pump();
        QCOMPARE(conn.speakerAmplifierMode(), 1);
        QCOMPARE(conn.radioSpeakerMuted(), false);
        QVERIFY(conn.log.contains(QStringLiteral("muted:1")));
        QVERIFY(conn.log.contains(QStringLiteral("mode:2")));
        // The volume is the engine's business; it never reaches the
        // connection, and no change sends MOX.
        for (const QString& entry : conn.log) {
            QVERIFY2(!entry.startsWith(QStringLiteral("mox")), qPrintable(entry));
        }
        model.injectConnectionForTest(nullptr);
    }

    void noConnection_macKnown_storesForNextConnect()
    {
        {
            RadioModel model;
            model.setLastRadioInfoForTest(radioInfo(kMacA));  // not connected
            model.setRadioSpeakerVolume(33);
            model.setRadioSpeakerMuted(true);
            QCOMPARE(key(kMacA, "Volume").toInt(), 33);
            QCOMPARE(key(kMacA, "Muted").toString(), QStringLiteral("True"));
        }
        RadioModel model;
        model.audioEngine()->setVolume(0.9f);
        LoggingConnection conn;
        model.setLastRadioInfoForTest(radioInfo(kMacA));
        model.injectConnectionForTest(&conn);
        const auto detach = qScopeGuard([&model] { model.injectConnectionForTest(nullptr); });
        model.loadRadioSpeakerForConnectForTest();
        QCOMPARE(model.radioSpeakerVolume(), 33);
        QCOMPARE(model.radioSpeakerMuted(), true);
        model.injectConnectionForTest(nullptr);
    }

    void noConnection_noMac_heldInMemory()
    {
        RadioModel model;
        model.audioEngine()->setVolume(0.9f);
        model.setRadioSpeakerVolume(25);
        QCOMPARE(model.radioSpeakerVolume(), 25);
        QVERIFY(!key(QString(), "Volume").isValid());
        QVERIFY(!key(kMacA, "Volume").isValid());

        // The first radio with nothing saved takes the held value.
        LoggingConnection conn;
        model.setLastRadioInfoForTest(radioInfo(kMacA));
        model.injectConnectionForTest(&conn);
        const auto detach = qScopeGuard([&model] { model.injectConnectionForTest(nullptr); });
        model.loadRadioSpeakerForConnectForTest();
        QCOMPARE(model.radioSpeakerVolume(), 25);
        QCOMPARE(key(kMacA, "Volume").toInt(), 25);

        // A later radio with nothing saved seeds from the master level again.
        model.setLastRadioInfoForTest(radioInfo(kMacB));
        model.loadRadioSpeakerForConnectForTest();
        QCOMPARE(model.radioSpeakerVolume(), 90);
        model.injectConnectionForTest(nullptr);
    }

    // ── R-SPK-15: the CW or Tune flag ───────────────────────────────────
    void cwMode_sendsSidetoneExpected()
    {
        RadioModel model;
        LoggingConnection conn;
        model.injectConnectionForTest(&conn);
        const auto detach = qScopeGuard([&model] { model.injectConnectionForTest(nullptr); });
        model.addSlice();
        SliceModel* slice = model.activeSlice();
        QVERIFY(slice != nullptr);
        slice->setDspMode(DSPMode::USB);
        pump();
        QCOMPARE(conn.sidetoneExpected(), false);

        slice->setDspMode(DSPMode::CWU);
        pump();
        QCOMPARE(conn.sidetoneExpected(), true);
        slice->setDspMode(DSPMode::CWL);
        pump();
        QCOMPARE(conn.sidetoneExpected(), true);
        slice->setDspMode(DSPMode::LSB);
        pump();
        QCOMPARE(conn.sidetoneExpected(), false);
        model.injectConnectionForTest(nullptr);
    }

    void tune_sendsFlagBeforeMoxOn_andClearsAfterMoxOff()
    {
        RadioModel model;
        model.setCapsForTest(/*hasAlex=*/false);
        auto conn = std::make_unique<LoggingConnection>();
        model.injectConnectionForTest(conn.get());
        const auto detach = qScopeGuard([&model] { model.injectConnectionForTest(nullptr); });
        model.moxController()->setTimerIntervals(0, 0, 0, 0, 0, 0);
        model.setTuneOffSettleMsForTest(0);
        model.addSlice();
        SliceModel* slice = model.activeSlice();
        QVERIFY(slice != nullptr);
        slice->setDspMode(DSPMode::USB);
        model.setSpeakerAmplifierMode(1);  // Off while transmitting
        pump();
        conn->log.clear();

        model.setTune(true);
        pump();
        QVERIFY(model.moxController()->isMox());
        const int flagOn = conn->log.indexOf(QStringLiteral("sidetone:1"));
        const int moxOn = conn->log.indexOf(QStringLiteral("mox:1"));
        QVERIFY2(flagOn >= 0, qPrintable(conn->log.join(u' ')));
        QVERIFY2(moxOn >= 0, qPrintable(conn->log.join(u' ')));
        QVERIFY2(flagOn < moxOn, qPrintable(conn->log.join(u' ')));
        QVERIFY(model.speakerAmplifierStatus().isEmpty());

        // The flag stays up until the radio has unkeyed, so the amplifier
        // never blinks off at the end of a Tune either.
        model.setTune(false);
        pump();
        QVERIFY(!model.moxController()->isMox());
        const int moxOff = conn->log.indexOf(QStringLiteral("mox:0"));
        const int flagOff = conn->log.indexOf(QStringLiteral("sidetone:0"));
        QVERIFY2(moxOff >= 0, qPrintable(conn->log.join(u' ')));
        QVERIFY2(flagOff > moxOff, qPrintable(conn->log.join(u' ')));
        QCOMPARE(conn->sidetoneExpected(), false);
        model.injectConnectionForTest(nullptr);
    }

    // A path that drops MOX before Tune clears (a PA trip, a MOX click
    // during Tune) leaves the radio walking to receive with MOX off not
    // yet sent. The flag still holds until it is.
    void tune_moxDroppedFirst_flagHeldUntilMoxOff()
    {
        RadioModel model;
        model.setCapsForTest(/*hasAlex=*/false);
        auto conn = std::make_unique<LoggingConnection>();
        model.injectConnectionForTest(conn.get());
        const auto detach = qScopeGuard([&model] { model.injectConnectionForTest(nullptr); });
        model.moxController()->setTimerIntervals(0, 0, 0, 0, 0, 0);
        model.setTuneOffSettleMsForTest(0);
        model.addSlice();
        SliceModel* slice = model.activeSlice();
        QVERIFY(slice != nullptr);
        slice->setDspMode(DSPMode::USB);
        model.setSpeakerAmplifierMode(1);  // Off while transmitting
        model.setTune(true);
        pump();
        QVERIFY(model.moxController()->isMox());
        QCOMPARE(conn->sidetoneExpected(), true);
        conn->log.clear();

        // MOX drops first; Tune clears while the unkey walk is in flight.
        model.moxController()->setMox(false);
        QVERIFY(!model.moxController()->isMox());
        QVERIFY(model.isTransmitting());
        model.setTune(false);
        pump();
        QVERIFY(!model.isTransmitting());
        const int moxOff = conn->log.indexOf(QStringLiteral("mox:0"));
        const int flagOff = conn->log.indexOf(QStringLiteral("sidetone:0"));
        QVERIFY2(moxOff >= 0, qPrintable(conn->log.join(u' ')));
        QVERIFY2(flagOff > moxOff, qPrintable(conn->log.join(u' ')));
        QCOMPARE(conn->sidetoneExpected(), false);
        model.injectConnectionForTest(nullptr);
    }

    // A voice key during the unkey walk after Tune cancels the walk, so
    // hardwareFlipped(false) never comes. The key itself ends the hold, so
    // the amplifier is off for that voice transmission in mode 1.
    void tune_rekeyDuringUnkeyWalk_clearsHold()
    {
        RadioModel model;
        model.setCapsForTest(/*hasAlex=*/false);
        auto conn = std::make_unique<LoggingConnection>();
        conn->protocol = 2;
        model.setHpsdrModelForTest(HPSDRModel::ANAN_G2);
        model.injectConnectionForTest(conn.get());
        const auto detach = qScopeGuard([&model] { model.injectConnectionForTest(nullptr); });
        model.moxController()->setTimerIntervals(0, 0, 0, 0, 0, 0);
        model.setTuneOffSettleMsForTest(0);
        model.addSlice();
        SliceModel* slice = model.activeSlice();
        QVERIFY(slice != nullptr);
        slice->setDspMode(DSPMode::USB);
        model.setSpeakerAmplifierMode(1);  // Off while transmitting
        model.setTune(true);
        pump();
        QVERIFY(model.moxController()->isMox());
        QCOMPARE(conn->sidetoneExpected(), true);

        // Tune ends; before the walk reaches hardwareFlipped(false) the
        // operator keys voice.
        model.setTune(false);
        QVERIFY(!model.moxController()->isMox());
        model.moxController()->setMox(true);
        pump();
        QVERIFY(model.moxController()->isMox());
        QVERIFY(!model.transmitModel().isTune());
        QCOMPARE(conn->sidetoneExpected(), false);
        QCOMPARE(model.speakerAmplifierStatus(),
                 QStringLiteral("Amplifier is off now: transmitting."));

        model.moxController()->setMox(false);
        pump();
        model.injectConnectionForTest(nullptr);
    }

    // ── V-SW-5 (local): availability and reasons ────────────────────────
    void noRadio_unavailable()
    {
        RadioModel model;
        QCOMPARE(model.radioSpeakerAvailability(), 0);
        QCOMPARE(model.radioSpeakerUnavailableReason(),
                 QStringLiteral("No radio connected"));
        QCOMPARE(model.speakerAmplifierAvailable(), false);
        QCOMPARE(model.speakerAmplifierUnavailableReason(),
                 QStringLiteral("No radio connected"));
        QVERIFY(model.speakerAmplifierStatus().isEmpty());
    }

    void availability_everyModelOnBothProtocols_data()
    {
        QTest::addColumn<int>("model");
        QTest::addColumn<int>("protocol");
        for (int m = int(HPSDRModel::FIRST) + 1; m < int(HPSDRModel::LAST); ++m) {
            for (int p : {1, 2}) {
                QTest::addRow("model%d_p%d", m, p) << m << p;
            }
        }
    }

    void availability_everyModelOnBothProtocols()
    {
        QFETCH(int, model);
        QFETCH(int, protocol);
        const auto m = static_cast<HPSDRModel>(model);

        RadioModel radio;
        LoggingConnection conn;
        conn.protocol = protocol;
        radio.setHpsdrModelForTest(m);
        QSignalSpy avail(&radio, &RadioModel::radioSpeakerAvailabilityChanged);
        QSignalSpy amp(&radio, &RadioModel::speakerAmplifierAvailableChanged);
        radio.injectConnectionForTest(&conn);
        const auto detach = qScopeGuard([&radio] { radio.injectConnectionForTest(nullptr); });

        const int expectedAvail = m == HPSDRModel::HERMESLITE ? 2 : 1;
        QCOMPARE(radio.radioSpeakerAvailability(), expectedAvail);
        QCOMPARE(avail.count(), 1);
        QVERIFY(radio.radioSpeakerUnavailableReason().isEmpty());

        const bool expectedAmp = protocol == 2 && hasAmplifier(m);
        QCOMPARE(radio.speakerAmplifierAvailable(), expectedAmp);
        QCOMPARE(amp.count(), expectedAmp ? 1 : 0);
        if (expectedAmp) {
            QVERIFY(radio.speakerAmplifierUnavailableReason().isEmpty());
        } else if (m == HPSDRModel::ANAN_G2E) {
            QCOMPARE(radio.speakerAmplifierUnavailableReason(),
                     QStringLiteral("Not tested on the ANAN-G2E."));
        } else {
            QCOMPARE(radio.speakerAmplifierUnavailableReason(),
                     QStringLiteral("This radio has no switchable speaker amplifier."));
        }

        // An amplifier board on Protocol 1 never reports the amplifier off.
        radio.setSpeakerAmplifierMode(2);
        if (expectedAmp) {
            QCOMPARE(radio.speakerAmplifierStatus(),
                     QStringLiteral("Amplifier is off now."));
        } else {
            QVERIFY(radio.speakerAmplifierStatus().isEmpty());
        }

        radio.injectConnectionForTest(nullptr);
        QCOMPARE(radio.radioSpeakerAvailability(), 0);
        QCOMPARE(radio.speakerAmplifierAvailable(), false);
        QVERIFY(radio.speakerAmplifierStatus().isEmpty());
    }

    void amplifierStatus_followsModeMuteAndKey()
    {
        RadioModel radio;
        radio.setCapsForTest(/*hasAlex=*/false);
        auto conn = std::make_unique<LoggingConnection>();
        conn->protocol = 2;
        radio.setHpsdrModelForTest(HPSDRModel::ANAN_G2);
        radio.injectConnectionForTest(conn.get());
        const auto detach = qScopeGuard([&radio] { radio.injectConnectionForTest(nullptr); });
        radio.moxController()->setTimerIntervals(0, 0, 0, 0, 0, 0);
        radio.addSlice();
        radio.activeSlice()->setDspMode(DSPMode::USB);
        QVERIFY(radio.speakerAmplifierAvailable());
        QSignalSpy status(&radio, &RadioModel::speakerAmplifierStatusChanged);

        QVERIFY(radio.speakerAmplifierStatus().isEmpty());
        radio.setSpeakerAmplifierMode(2);
        QCOMPARE(radio.speakerAmplifierStatus(), QStringLiteral("Amplifier is off now."));
        radio.setRadioSpeakerMuted(true);
        QCOMPARE(radio.speakerAmplifierStatus(),
                 QStringLiteral("Amplifier is off now: radio speaker muted."));
        radio.setRadioSpeakerMuted(false);
        radio.setSpeakerAmplifierMode(1);
        QVERIFY(radio.speakerAmplifierStatus().isEmpty());
        const int before = status.count();

        radio.moxController()->setMox(true);
        pump();
        QVERIFY(radio.isTransmitting());
        QCOMPARE(radio.speakerAmplifierStatus(),
                 QStringLiteral("Amplifier is off now: transmitting."));
        QVERIFY(status.count() > before);

        radio.activeSlice()->setDspMode(DSPMode::CWU);  // CW: side tone
        pump();
        QVERIFY(radio.speakerAmplifierStatus().isEmpty());

        radio.moxController()->setMox(false);
        pump();
        radio.activeSlice()->setDspMode(DSPMode::USB);
        pump();
        QVERIFY(radio.speakerAmplifierStatus().isEmpty());
        radio.injectConnectionForTest(nullptr);
    }

    // ── A real connect: never keys, saved level applied before the tap ──
    void realConnect_appliesSavedLevelBeforeTap_andNeverKeys()
    {
        AppSettings& s = AppSettings::instance();
        const QString mac = QStringLiteral("aa:bb:cc:11:22:33");  // the harness's
        s.setHardwareValue(mac, QStringLiteral("RadioSpeaker/Volume"), 55);

        bool appliedBeforeTap = false;
        bool sawLevel = false;
        auto harness = Test::ConnectableRadioModel::create(
            20000, RadioModel::Role::Local, [&](RadioModel& model) {
                QObject::connect(model.audioEngine(),
                                 &AudioEngine::radioSpeakerVolumeChanged, &model,
                                 [&model, &appliedBeforeTap, &sawLevel](float v) {
                    if (qAbs(v - 0.55f) > 1e-4f || sawLevel) {
                        return;
                    }
                    sawLevel = true;
                    // The tap is installed in wireConnectionSignals, after
                    // the connection has moved to its own thread.
                    RadioConnection* conn = model.connection();
                    appliedBeforeTap = conn != nullptr
                        && conn->thread() == QThread::currentThread();
                });
            });
        QVERIFY(harness);
        RadioModel& model = harness->model();
        QVERIFY(sawLevel);
        QVERIFY(appliedBeforeTap);
        QCOMPARE(model.radioSpeakerVolume(), 55);
        QCOMPARE(model.radioSpeakerAvailability(), 2);  // the harness's HL2
        QVERIFY(!key(mac, "Muted").isValid());
        QVERIFY(!key(mac, "AmplifierMode").isValid());

        QTRY_VERIFY_WITH_TIMEOUT(harness->fake().ep2FramesReceived() > 0, 5000);
        QVERIFY(!model.moxController()->isMox());
        QVERIFY(!model.isTransmitting());
        // P1 C0 bit 0 is MOX, in both subframes of every EP2 frame.
        for (const QByteArray& cc : harness->fake().ep2CcReceived()) {
            QCOMPARE(static_cast<quint8>(cc.at(0)) & 0x01, 0);
            QCOMPARE(static_cast<quint8>(cc.at(5)) & 0x01, 0);
        }
    }
};

QTEST_MAIN(TestRadioSpeakerModel)
#include "tst_radio_speaker_model.moc"
