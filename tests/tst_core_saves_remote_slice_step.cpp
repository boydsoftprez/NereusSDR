// =================================================================
// tests/tst_core_saves_remote_slice_step.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original test. A Core that has a radio saves a
// remote window's tune step for a slice that is not its active one, under
// that slice's own key, and brings it back.
//
// Found reviewing PR 517 (step persistence): its case for this marks the
// save as an expected failure, because the Core in that test has no radio
// connection and RadioModel::wireSliceSignals, which wires the per-slice
// step save, returns early without one. Here the Core's model has a radio
// connection before its slices are made, the way a Core that has its radio
// makes slices, so the save it wires is the one under test.
//
//   * the window sets the step of the Core's second slice (the Core's
//     active slice is the first); the Core saves it as
//     Slice1/Band<band>/StepHz, and the first slice's key keeps its own
//     step;
//   * a Core started again on the same settings, with its radio, brings
//     the step back on that slice, and a window that connects to it shows
//     the step.
//
// No radio and no audio device: a connection that sends nothing (the
// SilentRadioConnection of tst_radio_slice_settings_restore), a real
// StationServer, and a remote window's RadioModel and StationClient over
// the in-process loopback (tst_remote_tx_antennas). The settings store is
// the test sandbox's (TestSandboxInit).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-16  J.J. Boyd / KG4VCF  Created. AI-assisted via Anthropic
//                                    Claude Code.
//   2026-10-23  J.J. Boyd / KG4VCF  AGC-T AUTO and the step on each slice
//                                    across a Core restart. AI-assisted via
//                                    Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <QSignalSpy>
#include <QTemporaryDir>

#include <memory>

#include "core/AppSettings.h"
#include "core/RadioConnection.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "core/settings/SettingsProxy.h"
#include "models/Band.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include "fakes/LoopbackTransport.h"
#include "fakes/UpgradedCoreToken.h"

using namespace NereusSDR;
using NereusSDR::Test::LoopbackTransport;

// File scope, not an anonymous namespace: moc cannot generate a
// staticMetaObject for a Q_OBJECT class with internal linkage.
class SilentRadioConnection final : public RadioConnection {
    Q_OBJECT
public:
    explicit SilentRadioConnection(QObject* parent = nullptr)
        : RadioConnection(parent)
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
    void setTxDrive(int) override {}
    void setMox(bool) override {}
    void setAntennaRouting(AntennaRouting) override {}
    void setWatchdogEnabled(bool) override {}
    void sendTxIq(const float*, int) override {}
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

namespace {

constexpr int    kCoreStepHz   = 100;
constexpr int    kWindowStepHz = 1000;
constexpr double kSliceAFreqHz = 14200000.0;
constexpr double kSliceBFreqHz = 7100000.0;

QString stepKey(int sliceIndex, Band band)
{
    return QStringLiteral("Slice%1/Band%2/StepHz").arg(sliceIndex).arg(bandKeyName(band));
}

// A Core with its radio: the connection is there before the slices are
// made, so addSlice wires each slice's saves (wireSliceSignals) as it
// does on a Core whose radio is connected.
struct Core {
    explicit Core(const QString& tokenDir, bool restoreSavedState)
        : settings(dir.filePath(QStringLiteral("core.settings")))
    {
        // The Core's own store, migrated like the window's (init()).
        settings.setValue(QStringLiteral("SettingsSchemaVersion"), QStringLiteral("7"));
        model.injectConnectionForTest(&radio);
        model.addSlice(QStringLiteral("pan-0"), restoreSavedState);
        model.addSlice(QStringLiteral("pan-1"), restoreSavedState);
        server = std::make_unique<StationServer>(
            &model, settings, NereusSDR::Test::seedUpgradedCoreToken(tokenDir));
    }

    ~Core()
    {
        server.reset();
        model.injectConnectionForTest(nullptr);
    }

    Core(const Core&) = delete;
    Core& operator=(const Core&) = delete;

    QTemporaryDir dir;
    AppSettings settings;
    SilentRadioConnection radio;
    RadioModel model;
    std::unique_ptr<StationServer> server;
};

// A remote window's model joined to the Core over the loopback.
struct Window {
    Window() : client(&model, &proxy) {}

    bool join(Core& core, QObject* owner)
    {
        auto* stationEnd = new LoopbackTransport(QStringLiteral("step-station"), owner);
        auto* clientEnd = new LoopbackTransport(QStringLiteral("step-client"), owner);
        stationEnd->linkTo(clientEnd);
        QSignalSpy completed(&client, &StationClient::handshakeComplete);
        client.startSession(clientEnd, core.server->token());
        core.server->acceptTransport(stationEnd);
        return QTest::qWaitFor([&completed] { return !completed.isEmpty(); }, 5000);
    }

    RadioModel model{RadioModel::Role::Remote};
    SettingsProxy proxy;
    StationClient client;
};

} // namespace

class TestCoreSavesRemoteSliceStep : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_securityDir;

private slots:
    void initTestCase() { QVERIFY(m_securityDir.isValid()); }
    void init()
    {
        AppSettings::instance().clear();
        // As CoreInit's migrations leave it, so the window's StationClient
        // starts from a migrated profile (tst_cat_remote_setup).
        AppSettings::instance().setValue(QStringLiteral("SettingsSchemaVersion"),
                                         QStringLiteral("7"));
    }
    void cleanup() { AppSettings::instance().clear(); }

    void windowStepOnTheCoresOtherSliceIsSavedUnderThatSliceAndComesBack()
    {
        Band sliceBBand = Band::Band40m;
        {
            Core core(m_securityDir.path(), /*restoreSavedState=*/false);
            SliceModel* a = core.model.sliceById(0);
            SliceModel* b = core.model.sliceById(1);
            QVERIFY(a && b);
            QCOMPARE(core.model.activeSlice(), a);
            a->setFrequency(kSliceAFreqHz);
            b->setFrequency(kSliceBFreqHz);
            a->setStepHz(kCoreStepHz);
            b->setStepHz(kCoreStepHz);
            sliceBBand = b->band();
            core.model.flushPendingSettingsSave();
            const QString keyA = stepKey(0, a->band());
            const QString keyB = stepKey(1, sliceBBand);
            auto& s = AppSettings::instance();
            QVERIFY(s.value(keyB).toInt() != kWindowStepHz);

            Window window;
            QVERIFY(window.join(core, this));
            QTRY_VERIFY(window.model.sliceById(1) != nullptr);
            SliceModel* remoteB = window.model.sliceById(1);
            QTRY_COMPARE(remoteB->stepHz(), kCoreStepHz);

            remoteB->setStepHz(kWindowStepHz);
            QTRY_COMPARE(b->stepHz(), kWindowStepHz);
            QCOMPARE(core.model.activeSlice(), a);
            QCOMPARE(a->stepHz(), kCoreStepHz);

            // The Core saves the step under the slice it was set on.
            core.model.flushPendingSettingsSave();
            QCOMPARE(s.value(keyB).toInt(), kWindowStepHz);
            QCOMPARE(s.value(keyA).toInt(), kCoreStepHz);
            QCOMPARE(s.value(QStringLiteral("Slice1/LastBand")).toString(),
                     bandKeyName(sliceBBand));
        }

        // The Core starts again on the same settings and connects its
        // radio: the second slice comes back with the window's step, and a
        // window that connects shows it.
        Core core(m_securityDir.path(), /*restoreSavedState=*/true);
        SliceModel* a = core.model.sliceById(0);
        SliceModel* b = core.model.sliceById(1);
        QVERIFY(a && b);
        QCOMPARE(b->band(), sliceBBand);
        QCOMPARE(b->stepHz(), kWindowStepHz);
        QCOMPARE(a->stepHz(), kCoreStepHz);

        Window window;
        QVERIFY(window.join(core, this));
        QTRY_VERIFY(window.model.sliceById(1) != nullptr);
        QTRY_COMPARE(window.model.sliceById(1)->stepHz(), kWindowStepHz);
        QCOMPARE(window.model.sliceById(0)->stepHz(), kCoreStepHz);
    }

    // JJ's report 2026-10-23 (step and AGC auto do not persist between
    // launches): AGC-T AUTO and the step a window sets on each of the
    // Core's slices are saved under that slice and come back, on the Core
    // and in a window that connects to it, after the Core starts again.
    void windowAutoAndStepOnEachSliceComeBackAfterACoreRestart()
    {
        {
            Core core(m_securityDir.path(), /*restoreSavedState=*/false);
            SliceModel* a = core.model.sliceById(0);
            SliceModel* b = core.model.sliceById(1);
            QVERIFY(a && b);
            a->setFrequency(kSliceAFreqHz);
            b->setFrequency(kSliceBFreqHz);
            core.model.flushPendingSettingsSave();

            Window window;
            QVERIFY(window.join(core, this));
            QTRY_VERIFY(window.model.sliceById(1) != nullptr);
            SliceModel* remoteA = window.model.sliceById(0);
            SliceModel* remoteB = window.model.sliceById(1);
            QVERIFY(remoteA && remoteB);

            remoteA->setStepHz(kWindowStepHz);
            remoteA->setAutoAgcEnabled(true);
            remoteB->setAutoAgcEnabled(true);
            QTRY_COMPARE(a->stepHz(), kWindowStepHz);
            QTRY_VERIFY(a->autoAgcEnabled());
            QTRY_VERIFY(b->autoAgcEnabled());
            core.model.flushPendingSettingsSave();
            auto& s = AppSettings::instance();
            QCOMPARE(s.value(QStringLiteral("Slice0/AgcAutoEnabled")).toString(),
                     QStringLiteral("True"));
            QCOMPARE(s.value(QStringLiteral("Slice1/AgcAutoEnabled")).toString(),
                     QStringLiteral("True"));
        }

        Core core(m_securityDir.path(), /*restoreSavedState=*/true);
        SliceModel* a = core.model.sliceById(0);
        SliceModel* b = core.model.sliceById(1);
        QVERIFY(a && b);
        QCOMPARE(a->stepHz(), kWindowStepHz);
        QVERIFY(a->autoAgcEnabled());
        QVERIFY(b->autoAgcEnabled());

        Window window;
        QVERIFY(window.join(core, this));
        QTRY_VERIFY(window.model.sliceById(1) != nullptr);
        QTRY_COMPARE(window.model.sliceById(0)->stepHz(), kWindowStepHz);
        QTRY_VERIFY(window.model.sliceById(0)->autoAgcEnabled());
        QTRY_VERIFY(window.model.sliceById(1)->autoAgcEnabled());
    }
};

QTEST_MAIN(TestCoreSavesRemoteSliceStep)
#include "tst_core_saves_remote_slice_step.moc"
