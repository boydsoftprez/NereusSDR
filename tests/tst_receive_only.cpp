// no-port-check: NereusSDR test fixture; the Thetis behaviour it checks is
// cited inline and ported in MoxController / RadioModel / GeneralOptionsPage.
//
// =================================================================
// tests/tst_receive_only.cpp  (NereusSDR)
// =================================================================
//
// Receiver and transmit gaps plan, Task 16: receive only stops every key,
// as Thetis's RXOnly does.
//
// From Thetis console.cs:15312-15334 [v2.10.3.15] (RXOnly setter): MOX
// disabled unless SPEC or DRM, TUN, 2TONE (// MW0LGE_21a) and VOX disabled,
// a keyed MOX dropped, Setup kept in step. PollPTT skips every source while
// _rx_only is set (console.cs:25470) and chkMOX_CheckedChanged2 refuses any
// key (console.cs:29378). Setup's chkGeneralRXOnly_CheckedChanged
// (setup.cs:6479) asks before transmit is turned back on.
//
// Covered:
//   - every keying source is refused through the one gate (MoxController),
//     with a plain reason for the ones that report a refusal, and a keyed
//     transmission drops when receive only turns on;
//   - the HL2 receive-only kit (board byte 12, isRxOnlySku) always runs
//     receive only, and the Setup box shows checked and disabled with the
//     reason (NereusSDR's own rule; mi0bot-Thetis has no kit model);
//   - the Setup box is never hidden, asks before turning transmit on, and
//     follows the model;
//   - the TX applet's MOX, TUNE, 2-Tone and VOX and the container buttons
//     show disabled with the reason, MOX in every mode (fix wave I3: SPEC
//     and DRM too, where Thetis leaves it alone), following the active
//     slice (M3), with both reasons in a remote window (M6);
//   - Setup's Transmit and PA categories and Test > Two-Tone IMD are
//     disabled with the reason, never hidden (setup.cs:6499-6501, I1);
//   - two-tone started the way the applet starts it is refused and its
//     tone stopped (M4);
//   - the TGXL autotune is refused before it reaches the amplifier or the
//     tuner, and the Tuner applet's TUNE shows why (M2);
//   - a remote window's box reaches the Core's gate over a session, follows
//     the Core's value, and an older Core's refusal puts it back.
//
// No hardware; nothing keys a radio; no audio device is opened.
//
// Modification history (NereusSDR):
//   2026-09-25: created (Task 16), by J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-25: Task 16 fix wave (I1, I2, I3, M2, M3, M4, M6), by J.J.
//               Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-09-25: checkpoint carry: the remote Setup case follows parity
//               Task 5 (Two-Tone IMD no longer waits for remote transmit),
//               and the window signs in to an upgraded Core with its token
//               (seedUpgradedCoreToken). J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-29: slice control plan Task 11 (Q15): the MOX tooltip cases
//               follow the transmit slice, not the active one. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QTreeWidget>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>

#include <memory>

#include "core/AppSettings.h"
#include "core/HpsdrModel.h"
#include "core/MoxController.h"
#include "core/RadioConnection.h"
#include "core/TwoToneController.h"
#include "core/TxChannel.h"
#include "core/session/StationCapabilities.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "core/settings/SettingsProxy.h"
#include "gui/SetupDialog.h"
#include "gui/applets/TunerApplet.h"
#include "gui/applets/TxApplet.h"
#include "gui/containers/ContainerButtonDispatcher.h"
#include "gui/setup/GeneralOptionsPage.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TunerModel.h"

#include "OperatorWording.h"
#include "fakes/LoopbackTransport.h"
#include "fakes/UpgradedCoreToken.h"

using namespace NereusSDR;
using NereusSDR::Test::LoopbackTransport;

class RxOnlyMockConnection : public RadioConnection {
    Q_OBJECT
public:
    explicit RxOnlyMockConnection(QObject* parent = nullptr)
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
    void sendTxIq(const float*, int) override {}
    void setWatchdogEnabled(bool) override {}
    void setAntennaRouting(AntennaRouting) override {}
    void setMox(bool) override {}
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

const QString kKey = QStringLiteral("RxOnly");

void pump()
{
    for (int i = 0; i < 4; ++i) {
        QCoreApplication::processEvents();
    }
}

// A local radio: a connected mock, synchronous MOX walk, one USB slice.
struct Rig {
    std::unique_ptr<RxOnlyMockConnection> conn;
    std::unique_ptr<RadioModel> model;

    // capsOverride false: the board's own capability row, so
    // setBoardForTest reaches boardCapabilities() (the kit test).
    explicit Rig(bool capsOverride = true)
        : conn(std::make_unique<RxOnlyMockConnection>())
        , model(std::make_unique<RadioModel>())
    {
        if (capsOverride) {
            model->setCapsForTest(/*hasAlex=*/false);
        }
        model->injectConnectionForTest(conn.get());
        model->moxController()->setTimerIntervals(0, 0, 0, 0, 0, 0);
        model->setTuneOffSettleMsForTest(0);
        model->addSlice();
        model->activeSlice()->setDspMode(DSPMode::USB);
    }

    ~Rig()
    {
        if (model->isTune()) {
            model->setTune(false);
            pump();
        }
        model->injectConnectionForTest(nullptr);
        model.reset();
    }

    MoxController* mox() const { return model->moxController(); }
};

// Keys one source the way it keys in production.
void keySource(Rig& rig, const QString& source)
{
    MoxController* mox = rig.mox();
    if (source == QLatin1String("mic")) {
        mox->onMicPttFromRadio(true);
    } else if (source == QLatin1String("vox")) {
        mox->onVoxActive(true);
    } else if (source == QLatin1String("cat")) {
        mox->onCatPtt(true);
    } else if (source == QLatin1String("tci")) {
        rig.model->setMox(true);            // TciProtocol's trx shim
    } else if (source == QLatin1String("mox button")) {
        rig.model->setMoxFromButton(true);  // TxApplet and the container MOX
    } else if (source == QLatin1String("tun")) {
        rig.model->setTune(true);
    } else if (source == QLatin1String("two-tone")) {
        // Two-tone keys with the manual key and setMox(true).
        mox->setManualKey(true);
        mox->setMox(true);
    } else if (source == QLatin1String("space")) {
        mox->onSpacePtt(true);
    }
}

class ScopedRemoteBackend {
public:
    explicit ScopedRemoteBackend(ISettingsBackend* backend)
    {
        AppSettings::instance().setRemoteBackend(backend);
    }
    ~ScopedRemoteBackend() { AppSettings::instance().setRemoteBackend(nullptr); }
};

QCheckBox* rxOnlyBox(GeneralOptionsPage& page)
{
    return page.findChild<QCheckBox*>(QStringLiteral("chkGeneralRXOnly"));
}

// A TX channel that records the two-tone generator's run state and touches
// no WDSP channel (the TwoToneController test's pattern).
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

// The Setup tree row labelled `label` (a category when `category`).
QTreeWidgetItem* setupRow(SetupDialog& dialog, const QString& label, bool category)
{
    auto* tree = dialog.findChild<QTreeWidget*>();
    if (tree == nullptr) {
        return nullptr;
    }
    for (QTreeWidgetItemIterator it(tree); *it; ++it) {
        if (((*it)->parent() == nullptr) == category && (*it)->text(0) == label) {
            return *it;
        }
    }
    return nullptr;
}

QPushButton* tunerTuneButton(TunerApplet& applet)
{
    for (QPushButton* b : applet.findChildren<QPushButton*>()) {
        if (b->text() == QLatin1String("TUNE")) {
            return b;
        }
    }
    return nullptr;
}

} // namespace

class TestReceiveOnly : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_securityDir;

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_securityDir.isValid());
    }
    void init() { AppSettings::instance().clear(); }
    void cleanup() { AppSettings::instance().clear(); }

    // ---- The gate: every source refused -----------------------------------

    void everySourceIsRefused_data()
    {
        QTest::addColumn<QString>("source");
        QTest::addColumn<bool>("reported");
        // The PollPTT sources are skipped silently, as Thetis's poll is
        // (console.cs:25470); every other key reports why.
        QTest::newRow("mic") << QStringLiteral("mic") << false;
        QTest::newRow("vox") << QStringLiteral("vox") << false;
        QTest::newRow("cat") << QStringLiteral("cat") << false;
        QTest::newRow("tci") << QStringLiteral("tci") << false;
        QTest::newRow("mox button") << QStringLiteral("mox button") << true;
        QTest::newRow("tun") << QStringLiteral("tun") << true;
        QTest::newRow("two-tone") << QStringLiteral("two-tone") << true;
        QTest::newRow("space") << QStringLiteral("space") << true;
    }

    void everySourceIsRefused()
    {
        QFETCH(QString, source);
        QFETCH(bool, reported);
        Rig rig;
        rig.model->setRxOnly(true);
        QVERIFY(rig.model->isRxOnly());
        QVERIFY(rig.mox()->isRxOnly());

        QSignalSpy rejected(rig.mox(), &MoxController::moxRejected);
        keySource(rig, source);
        pump();
        QVERIFY2(!rig.mox()->isMox(), qPrintable(source + QStringLiteral(" keyed in receive only")));
        QVERIFY(!rig.model->isTune());
        if (reported) {
            QVERIFY2(!rejected.isEmpty(), qPrintable(source + QStringLiteral(" was not told why")));
            const QString reason = rejected.last().at(0).toString();
            QCOMPARE(reason, rig.model->rxOnlyReason());
            QVERIFY(OperatorWording::isPlain(reason));
        } else {
            QVERIFY(rejected.isEmpty());
        }
    }

    // CAT and TCI requests are dropped, not held: they do not key when
    // receive only is turned off (as under TX inhibit, Task 7 follow-up N3).
    void appRequestsAreDroppedNotHeld_data()
    {
        QTest::addColumn<QString>("source");
        for (const char* source : {"cat", "tci", "mox button", "tun"}) {
            QTest::newRow(source) << QString::fromLatin1(source);
        }
    }

    void appRequestsAreDroppedNotHeld()
    {
        QFETCH(QString, source);
        Rig rig;
        rig.model->setRxOnly(true);
        keySource(rig, source);
        pump();
        rig.model->setRxOnly(false);
        pump();
        QVERIFY(!rig.mox()->isRxOnly());
        // Task 16 fix wave (I2): PollPTT runs on events, so a held CAT or
        // TCI level would key on the next pass, which in production is the
        // next mic frame. Drive passes, as Task 7's test does
        // (tst_mox_controller_ptt_sources, catTciUnderBlock_areDroppedNotHeld).
        for (int i = 0; i < 3; ++i) {
            rig.mox()->onMicPttFromRadio(false);
            pump();
        }
        QVERIFY2(!rig.mox()->isMox(),
                 qPrintable(source + QStringLiteral(" keyed when receive only went off")));
        QCOMPARE(rig.mox()->pttMode(), PttMode::None);
    }

    // From Thetis console.cs:15325-15326 [v2.10.3.15]:
    //   if (_rx_only && chkMOX.Checked)
    //       chkMOX.Checked = false;
    void turningOnDropsAKeyedTransmission_data()
    {
        QTest::addColumn<QString>("source");
        for (const char* source : {"mox button", "mic", "tun", "tci"}) {
            QTest::newRow(source) << QString::fromLatin1(source);
        }
    }

    void turningOnDropsAKeyedTransmission()
    {
        QFETCH(QString, source);
        Rig rig;
        keySource(rig, source);
        pump();
        QVERIFY2(rig.mox()->isMox(), qPrintable(source + QStringLiteral(" did not key")));

        rig.model->setRxOnly(true);
        pump();
        QVERIFY2(!rig.mox()->isMox(), qPrintable(source + QStringLiteral(" stayed keyed")));
        QVERIFY(!rig.model->isTune());
    }

    // Turned off, keying works again: a new press keys.
    void turningOffLetsANewPressKey()
    {
        Rig rig;
        rig.model->setRxOnly(true);
        rig.model->setRxOnly(false);
        QVERIFY(!rig.model->isRxOnly());
        rig.model->setMoxFromButton(true);
        pump();
        QVERIFY(rig.mox()->isMox());
        rig.model->setMoxFromButton(false);
        pump();
        QVERIFY(!rig.mox()->isMox());
    }

    void settingIsSavedAndReadAtStart()
    {
        {
            Rig rig;
            QSignalSpy changed(rig.model.get(), &RadioModel::rxOnlyChanged);
            rig.model->setRxOnly(true);
            QCOMPARE(AppSettings::instance().value(kKey).toString(), QStringLiteral("True"));
            QCOMPARE(changed.count(), 1);
        }
        // Thetis restores it at start (setup.cs:740 [v2.10.3.15]).
        RadioModel fresh;
        QVERIFY(fresh.isRxOnly());
        QVERIFY(fresh.moxController()->isRxOnly());
    }

    // ---- The HL2 receive-only kit -----------------------------------------

    void receiveOnlyKitAlwaysRunsReceiveOnly()
    {
        Rig rig(/*capsOverride=*/false);
        rig.model->setBoardForTest(HPSDRHW::HermesLiteRxOnly);
        QVERIFY(rig.model->boardCapabilities().isRxOnlySku);
        QVERIFY(rig.model->isRxOnly());
        QVERIFY(rig.model->isRxOnlyForced());
        QVERIFY(rig.mox()->isRxOnly());
        QCOMPARE(rig.model->rxOnlyReason(), RadioModel::rxOnlyForcedReason());
        QVERIFY(OperatorWording::isPlain(rig.model->rxOnlyReason()));
        QVERIFY(rig.model->rxOnlyReason().contains(QStringLiteral("no transmitter")));

        // The setting cannot turn it off.
        rig.model->setRxOnly(false);
        QVERIFY(rig.model->isRxOnly());
        QSignalSpy rejected(rig.mox(), &MoxController::moxRejected);
        rig.model->setMoxFromButton(true);
        pump();
        QVERIFY(!rig.mox()->isMox());
        QCOMPARE(rejected.last().at(0).toString(), RadioModel::rxOnlyForcedReason());

        // A standard HL2 follows the setting again.
        rig.model->setBoardForTest(HPSDRHW::HermesLite);
        QVERIFY(!rig.model->isRxOnly());
        QVERIFY(!rig.model->isRxOnlyForced());
        QVERIFY(!rig.mox()->isRxOnly());
    }

    // ---- Setup > General > Options: Receive Only ---------------------------

    void checkboxIsNeverHidden_data()
    {
        QTest::addColumn<int>("board");
        QTest::newRow("no radio") << -1;
        QTest::newRow("HL2") << int(HPSDRHW::HermesLite);
        QTest::newRow("kit") << int(HPSDRHW::HermesLiteRxOnly);
        QTest::newRow("G2") << int(HPSDRHW::OrionMKII);
        QTest::newRow("Hermes") << int(HPSDRHW::Hermes);
    }

    void checkboxIsNeverHidden()
    {
        QFETCH(int, board);
        RadioModel model;
        if (board >= 0) {
            model.setBoardForTest(static_cast<HPSDRHW>(board));
        }
        GeneralOptionsPage page(board >= 0 ? &model : nullptr);
        QCheckBox* box = rxOnlyBox(page);
        QVERIFY(box != nullptr);
        QVERIFY2(!box->isHidden(), "Receive Only must be shown on every radio");
        QCOMPARE(box->text(), QStringLiteral("Receive Only"));
    }

    void kitShowsCheckedAndDisabledWithTheReason()
    {
        RadioModel model;
        model.setBoardForTest(HPSDRHW::HermesLiteRxOnly);
        GeneralOptionsPage page(&model);
        QCheckBox* box = rxOnlyBox(page);
        QVERIFY(box != nullptr);
        QVERIFY(box->isChecked());
        QVERIFY(!box->isEnabled());
        QCOMPARE(box->toolTip(), RadioModel::rxOnlyForcedReason());
        QVERIFY(OperatorWording::isPlain(box->toolTip()));

        // Back on a radio with a transmitter: the setting, enabled.
        model.setBoardForTest(HPSDRHW::HermesLite);
        model.emitCurrentRadioChangedForTest();
        QVERIFY(!box->isChecked());
        QVERIFY(box->isEnabled());
        QCOMPARE(box->toolTip(), QStringLiteral("Check to disable transmit functionality."));
    }

    // From Thetis setup.cs:6481-6498 [v2.10.3.15]: unchecking asks first, and
    // No puts the check back.
    void turningOffAsksFirst()
    {
        Rig rig;
        GeneralOptionsPage page(rig.model.get());
        QCheckBox* box = rxOnlyBox(page);
        QVERIFY(box != nullptr);
        int asked = 0;
        bool answer = false;
        page.setEnableTransmitConfirmForTest([&] { ++asked; return answer; });

        box->click();                  // on: no question
        QCOMPARE(asked, 0);
        QVERIFY(rig.model->isRxOnly());
        QCOMPARE(AppSettings::instance().value(kKey).toString(), QStringLiteral("True"));

        box->click();                  // off, answered No
        QCOMPARE(asked, 1);
        QVERIFY(box->isChecked());
        QVERIFY(rig.model->isRxOnly());

        answer = true;
        box->click();                  // off, answered Yes
        QCOMPARE(asked, 2);
        QVERIFY(!box->isChecked());
        QVERIFY(!rig.model->isRxOnly());
        QCOMPARE(AppSettings::instance().value(kKey).toString(), QStringLiteral("False"));
    }

    // Thetis keeps Setup in step with console.RXOnly (console.cs:15328-15332).
    void checkboxFollowsTheModel()
    {
        Rig rig;
        GeneralOptionsPage page(rig.model.get());
        QCheckBox* box = rxOnlyBox(page);
        QVERIFY(box != nullptr);
        QVERIFY(!box->isChecked());
        rig.model->setRxOnly(true);
        QVERIFY(box->isChecked());
        rig.model->applyRxOnlySetting(false);
        QVERIFY(!box->isChecked());
    }

    // ---- The transmit buttons ----------------------------------------------

    void txAppletButtonsAreDisabledWithTheReason()
    {
        Rig rig;
        TxApplet applet(rig.model.get());
        QPushButton* mox = applet.moxButton();
        QPushButton* tune = applet.tuneButton();
        QPushButton* twoTone = applet.twoToneButton();
        QPushButton* vox = applet.voxButton();
        QVERIFY(mox && tune && twoTone && vox);
        const QString moxTip = mox->toolTip();
        const QString tuneTip = tune->toolTip();
        QVERIFY(mox->isEnabled() && tune->isEnabled() && twoTone->isEnabled()
                && vox->isEnabled());

        rig.model->setRxOnly(true);
        for (QPushButton* b : {mox, tune, twoTone, vox}) {
            QVERIFY2(!b->isEnabled(), qPrintable(b->text() + QStringLiteral(" left enabled")));
            QCOMPARE(b->toolTip(), rig.model->rxOnlyReason());
            QVERIFY(OperatorWording::isPlain(b->toolTip()));
        }

        rig.model->setRxOnly(false);
        for (QPushButton* b : {mox, tune, twoTone, vox}) {
            QVERIFY(b->isEnabled());
        }
        QCOMPARE(mox->toolTip(), moxTip);
        QCOMPARE(tune->toolTip(), tuneTip);
    }

    // Thetis console.cs:15318-15321 [v2.10.3.15] leaves chkMOX.Enabled alone
    // in SPEC and DRM. NereusSDR disables MOX with the reason in every mode
    // (fix wave I3: the gate refuses the key in every mode, and a control
    // that cannot run is shown disabled with its reason).
    void moxButtonIsDisabledInEveryMode_data()
    {
        QTest::addColumn<int>("mode");
        QTest::newRow("SPEC") << int(DSPMode::SPEC);
        QTest::newRow("DRM") << int(DSPMode::DRM);
        QTest::newRow("USB") << int(DSPMode::USB);
    }

    void moxButtonIsDisabledInEveryMode()
    {
        QFETCH(int, mode);
        Rig rig;
        TxApplet applet(rig.model.get());
        ContainerButtonDispatcher::Hooks hooks;
        hooks.transmitPermitted = [] { return true; };
        ContainerButtonDispatcher dispatcher(rig.model.get(), std::move(hooks));
        rig.model->activeSlice()->setDspMode(static_cast<DSPMode>(mode));
        rig.model->setRxOnly(true);
        QVERIFY(rig.model->receiveOnlyDisablesMoxButton());
        QVERIFY(!applet.moxButton()->isEnabled());
        QCOMPARE(applet.moxButton()->toolTip(), rig.model->rxOnlyReason());
        const auto st = dispatcher.stateOf(ContainerButtonDispatcher::Id::Mox, 0);
        QVERIFY(!st.available);
        QCOMPARE(st.reason, rig.model->rxOnlyReason());

        // A mode change keeps it disabled with the reason.
        rig.model->activeSlice()->setDspMode(DSPMode::LSB);
        QVERIFY(!applet.moxButton()->isEnabled());
        rig.model->activeSlice()->setDspMode(static_cast<DSPMode>(mode));
        QVERIFY(!applet.moxButton()->isEnabled());
        QCOMPARE(applet.moxButton()->toolTip(), rig.model->rxOnlyReason());

        // The gate still refuses.
        rig.model->setMoxFromButton(true);
        pump();
        QVERIFY(!rig.mox()->isMox());

        // Off: MOX is back, with its mode's tooltip.
        rig.model->setRxOnly(false);
        QVERIFY(applet.moxButton()->isEnabled());
        QCOMPARE(applet.moxButton()->toolTip(),
                 TxApplet::tooltipForMode(static_cast<DSPMode>(mode)));
    }

    // Fix wave M3: the MOX tooltip, and the lock over it, follow the slice
    // when it changes, not the slice at construction. Slice control plan
    // Task 11 (Q15): that is the transmit slice, so moving only the active
    // slice leaves the tooltip on the transmit slice's mode.
    void txAppletMoxFollowsTheTransmitSlice()
    {
        Rig rig;
        rig.model->addSlice();
        QCOMPARE(rig.model->slices().size(), 2);
        SliceModel* first = rig.model->slices().at(0);
        SliceModel* second = rig.model->slices().at(1);
        rig.model->setActiveSlice(0);
        QVERIFY(rig.model->requestTxHandoffToSlice(first->sliceIndex()));
        first->setDspMode(DSPMode::USB);
        second->setDspMode(DSPMode::CWU);
        TxApplet applet(rig.model.get());
        QPushButton* mox = applet.moxButton();
        QCOMPARE(mox->toolTip(), TxApplet::tooltipForMode(DSPMode::USB));

        rig.model->setActiveSlice(1);
        QCOMPARE(mox->toolTip(), TxApplet::tooltipForMode(DSPMode::USB));
        QVERIFY(rig.model->requestTxHandoffToSlice(second->sliceIndex()));
        QCOMPARE(mox->toolTip(), TxApplet::tooltipForMode(DSPMode::CWU));
        // The new slice's mode changes reach the tooltip; the old one's do not.
        second->setDspMode(DSPMode::FM);
        QCOMPARE(mox->toolTip(), TxApplet::tooltipForMode(DSPMode::FM));
        first->setDspMode(DSPMode::LSB);
        QCOMPARE(mox->toolTip(), TxApplet::tooltipForMode(DSPMode::FM));

        // Under receive only the lock stays on across a slice change, and
        // comes off to the new slice's tooltip.
        rig.model->setRxOnly(true);
        QVERIFY(rig.model->requestTxHandoffToSlice(first->sliceIndex()));
        QVERIFY(!mox->isEnabled());
        QCOMPARE(mox->toolTip(), rig.model->rxOnlyReason());
        rig.model->setRxOnly(false);
        QVERIFY(mox->isEnabled());
        QCOMPARE(mox->toolTip(), TxApplet::tooltipForMode(DSPMode::LSB));
    }

    // The lock sits on top of a remote window's transmit-permission gate.
    void txAppletLockStacksOnTheTransmitPermission()
    {
        Rig rig;
        TxApplet applet(rig.model.get());
        QPushButton* tune = applet.tuneButton();
        const QString tuneTip = tune->toolTip();
        rig.model->setRxOnly(true);
        applet.setTransmitPermitted(false, QStringLiteral("Waiting for the Core."));
        QVERIFY(!tune->isEnabled());
        // Fix wave M6: both reasons, so turning receive only off does not
        // leave TUNE blocked for a reason never shown.
        QCOMPARE(tune->toolTip(),
                 rig.model->rxOnlyReason() + QStringLiteral(" Waiting for the Core."));
        rig.model->setRxOnly(false);
        QVERIFY(!tune->isEnabled());
        QCOMPARE(tune->toolTip(), QStringLiteral("Waiting for the Core."));
        applet.setTransmitPermitted(true);
        QVERIFY(tune->isEnabled());
        QCOMPARE(tune->toolTip(), tuneTip);
    }

    // Fix wave 2 (Minor 2): a slice or mode change while the remote
    // transmit-permission layer holds MOX keeps its reason on the button,
    // and the new mode's tooltip comes back with permission.
    void txAppletModeChangeKeepsThePermissionReason()
    {
        Rig rig;
        rig.model->addSlice();
        SliceModel* second = rig.model->slices().at(1);
        second->setDspMode(DSPMode::CWU);
        TxApplet applet(rig.model.get());
        QPushButton* mox = applet.moxButton();
        const QString waiting = QStringLiteral("Waiting for the Core.");
        applet.setTransmitPermitted(false, waiting);
        QVERIFY(!mox->isEnabled());
        QCOMPARE(mox->toolTip(), waiting);

        // Slice control plan Task 11: the tooltip follows the transmit slice.
        QVERIFY(rig.model->requestTxHandoffToSlice(second->sliceIndex()));
        QCOMPARE(mox->toolTip(), waiting);
        second->setDspMode(DSPMode::FM);
        QCOMPARE(mox->toolTip(), waiting);
        QVERIFY(!mox->isEnabled());

        applet.setTransmitPermitted(true);
        QVERIFY(mox->isEnabled());
        QCOMPARE(mox->toolTip(), TxApplet::tooltipForMode(DSPMode::FM));
    }

    void containerButtonsAreUnavailableWithTheReason()
    {
        Rig rig;
        ContainerButtonDispatcher::Hooks hooks;
        hooks.transmitPermitted = [] { return true; };
        ContainerButtonDispatcher dispatcher(rig.model.get(), std::move(hooks));
        using Id = ContainerButtonDispatcher::Id;
        for (Id id : {Id::Tun, Id::Mox, Id::TwoTon}) {
            QVERIFY(dispatcher.stateOf(id, 0).available);
        }

        rig.model->setRxOnly(true);
        for (Id id : {Id::Tun, Id::Mox, Id::TwoTon}) {
            const auto st = dispatcher.stateOf(id, 0);
            QVERIFY(!st.available);
            QCOMPARE(st.reason, rig.model->rxOnlyReason());
        }
        // A click says why and keys nothing.
        QCOMPARE(dispatcher.click(Id::Mox, 0), rig.model->rxOnlyReason());
        pump();
        QVERIFY(!rig.mox()->isMox());

        // SPEC: MOX stays unavailable (fix wave I3).
        rig.model->activeSlice()->setDspMode(DSPMode::SPEC);
        QVERIFY(!dispatcher.stateOf(Id::Mox, 0).available);
        QVERIFY(!dispatcher.stateOf(Id::Tun, 0).available);
    }

    // Fix wave M6: in a remote window where receive only and the missing
    // remote transmit both apply, the container names both.
    void containerNamesBothReasonsInARemoteWindow()
    {
        RadioModel window(RadioModel::Role::Remote);
        ContainerButtonDispatcher::Hooks hooks;
        hooks.transmitPermitted = [] { return false; };
        hooks.remoteTransmitReason = QStringLiteral("Remote transmit is not available yet.");
        ContainerButtonDispatcher dispatcher(&window, std::move(hooks));
        using Id = ContainerButtonDispatcher::Id;
        window.applyRxOnlySetting(true);
        for (Id id : {Id::Tun, Id::Mox, Id::TwoTon}) {
            const auto st = dispatcher.stateOf(id, 0);
            QVERIFY(!st.available);
            QVERIFY2(st.reason.contains(window.rxOnlyReason()), qPrintable(st.reason));
            QVERIFY2(st.reason.contains(QStringLiteral("Remote transmit is not available yet.")),
                     qPrintable(st.reason));
            QVERIFY(OperatorWording::isPlain(st.reason));
        }
        window.applyRxOnlySetting(false);
        QCOMPARE(dispatcher.stateOf(Id::Mox, 0).reason,
                 QStringLiteral("Remote transmit is not available yet."));
    }

    // ---- Two-tone through the production path (fix wave M4) ----------------

    // TxApplet and the container start two-tone with
    // TwoToneController::setActive(true) (TxApplet.cpp, the 2-Tone button;
    // ContainerButtonDispatcher, 2TONE). Receive only refuses its key, and
    // the tone it started for that key stops.
    void twoToneStartedTheProductionWayIsRefusedAndItsToneStops()
    {
        Rig rig;
        ToneRecordingTxChannel tone;
        TwoToneController* twoTone = rig.model->twoToneController();
        QVERIFY(twoTone != nullptr);
        twoTone->setTxChannel(&tone);
        twoTone->setSliceModel(rig.model->activeSlice());
        twoTone->setSettleDelaysMs(0, 0);
        twoTone->setPowerOn(true);
        rig.model->setRxOnly(true);

        QSignalSpy rejected(rig.mox(), &MoxController::moxRejected);
        twoTone->setActive(true);
        for (int i = 0; i < 10; ++i) {
            pump();
        }
        QVERIFY(!rig.mox()->isMox());
        QVERIFY(!twoTone->isActive());
        QVERIFY(!twoTone->isActivationInFlight());
        QVERIFY2(!tone.runs.isEmpty() && !tone.runs.last(), "the two-tone generator was left running");
        QVERIFY(!rejected.isEmpty());
        QCOMPARE(rejected.last().at(0).toString(), rig.model->rxOnlyReason());
        twoTone->setTxChannel(nullptr);
    }

    // ---- Setup's transmit pages (fix wave I1) ------------------------------

    // From Thetis setup.cs:6499-6501 [v2.10.3.15]:
    //   tpTransmit.Enabled = !chkGeneralRXOnly.Checked;
    //   tpPowerAmplifier.Enabled = !chkGeneralRXOnly.Checked;
    //   grpTestTXIMD.Enabled = !chkGeneralRXOnly.Checked;
    // Disabled with the reason, never hidden, following rxOnlyChanged.
    void setupTransmitPagesAreDisabledWithTheReason_data()
    {
        QTest::addColumn<QString>("label");
        QTest::addColumn<QString>("category");
        for (const char* page : {"Power", "Speech Processor", "DEXP/VOX"}) {
            QTest::newRow(page) << QString::fromLatin1(page) << QStringLiteral("Transmit");
        }
        for (const char* page : {"PA Gain", "Watt Meter", "PA Values"}) {
            QTest::newRow(page) << QString::fromLatin1(page) << QStringLiteral("PA");
        }
        QTest::newRow("Two-Tone IMD") << QStringLiteral("Two-Tone IMD") << QStringLiteral("Test");
        // Thetis's grpTXProfile is on tpTransmit (setup.designer.cs:46448).
        QTest::newRow("TX Profile") << QStringLiteral("TX Profile") << QStringLiteral("Audio");
    }

    void setupTransmitPagesAreDisabledWithTheReason()
    {
        QFETCH(QString, label);
        QFETCH(QString, category);
        // A radio with power amplifier settings (an ANAN-G2), so the PA
        // pages are live until receive only disables them.
        Rig rig(/*capsOverride=*/false);
        rig.model->setBoardForTest(HPSDRHW::Saturn);
        QVERIFY(rig.model->boardCapabilities().hasPaProfile);
        SetupDialog dialog(rig.model.get());
        dialog.selectPage(label);
        QWidget* page = dialog.realizePageForTest(label);
        QVERIFY(page != nullptr);
        QVERIFY(page->isEnabled());
        QTreeWidgetItem* leaf = setupRow(dialog, label, /*category=*/false);
        QVERIFY(leaf != nullptr);
        auto* notice = dialog.findChild<QLabel*>(QStringLiteral("setupReceiveOnly"));
        QVERIFY(notice != nullptr);

        rig.model->setRxOnly(true);
        const QString reason = rig.model->rxOnlyReason();
        QVERIFY2(!page->isEnabled(), qPrintable(label + QStringLiteral(" left enabled")));
        QCOMPARE(page->toolTip(), reason);
        QCOMPARE(leaf->toolTip(0), reason);
        // Never hidden (fix wave 2, Important 2: PA too), with the notice
        // above the page.
        QVERIFY(!leaf->isHidden());
        QVERIFY(page->isVisibleTo(&dialog));
        QVERIFY(!notice->isHidden());
        QCOMPARE(notice->text(), reason);
        if (category == QLatin1String("Transmit") || category == QLatin1String("PA")) {
            QTreeWidgetItem* root = setupRow(dialog, category, /*category=*/true);
            QVERIFY(root != nullptr);
            QVERIFY(!root->isHidden());
            QCOMPARE(root->toolTip(0), reason);
        }
        QVERIFY(OperatorWording::isPlain(reason));

        rig.model->setRxOnly(false);
        QVERIFY(page->isEnabled());
        QVERIFY(page->toolTip().isEmpty());
        QVERIFY(leaf->toolTip(0).isEmpty());
        QVERIFY(notice->isHidden());
    }

    // Fix wave 2 (Minor 4): Audio > TX Input follows receive only, as
    // Thetis's grpBoxMic on tpTransmit does (setup.designer.cs:46443
    // [v2.10.3.15]; setup.cs:6499 [v2.10.3.15]), with the reason.
    void setupTxInputFollowsReceiveOnly()
    {
        Rig rig;
        SetupDialog dialog(rig.model.get());
        const QString label = QStringLiteral("TX Input");
        dialog.selectPage(label);
        QWidget* page = dialog.realizedPageForTest(label);
        QVERIFY(page != nullptr);
        QVERIFY(page->isEnabled());
        QTreeWidgetItem* leaf = setupRow(dialog, label, /*category=*/false);
        QVERIFY(leaf != nullptr);
        auto* notice = dialog.findChild<QLabel*>(QStringLiteral("setupReceiveOnly"));
        QVERIFY(notice != nullptr);

        rig.model->setRxOnly(true);
        const QString reason = rig.model->rxOnlyReason();
        QVERIFY(!page->isEnabled());
        QCOMPARE(page->toolTip(), reason);
        QCOMPARE(leaf->toolTip(0), reason);
        QVERIFY(!leaf->isHidden());
        QVERIFY(!notice->isHidden());
        QCOMPARE(notice->text(), reason);

        rig.model->setRxOnly(false);
        QVERIFY(page->isEnabled());
        QVERIFY(page->toolTip().isEmpty());
        QVERIFY(leaf->toolTip(0).isEmpty());
        QVERIFY(notice->isHidden());
    }

    // Fix wave 2 (Minor 4): a remote window without transmit keeps TX Input
    // live (R-R3-36); only receive only disables it, with its own reason.
    void setupTxInputInARemoteWindowOnlyFollowsReceiveOnly()
    {
        const QString transmitReason = QStringLiteral("Remote transmit is unavailable.");
        RadioModel window(RadioModel::Role::Remote);
        SetupDialog dialog(&window);
        dialog.setTransmitPermitted(false, transmitReason);
        const QString label = QStringLiteral("TX Input");
        dialog.selectPage(label);
        QWidget* page = dialog.realizedPageForTest(label);
        QVERIFY(page != nullptr);
        QVERIFY(page->isEnabled());

        window.applyRxOnlySetting(true);
        QVERIFY(!page->isEnabled());
        QCOMPARE(page->toolTip(), window.rxOnlyReason());
        QCOMPARE(setupRow(dialog, label, /*category=*/false)->toolTip(0), window.rxOnlyReason());

        window.applyRxOnlySetting(false);
        QVERIFY(page->isEnabled());
        QVERIFY(page->toolTip().isEmpty());
    }

    // A receive page stays live; only the transmit pages follow.
    void setupReceivePagesStayEnabled()
    {
        Rig rig;
        SetupDialog dialog(rig.model.get());
        QWidget* page = dialog.realizePageForTest(QStringLiteral("NB/SNB"));
        QVERIFY(page != nullptr);
        rig.model->setRxOnly(true);
        QVERIFY(page->isEnabled());
    }

    void setupShowsTheKitsReason()
    {
        RadioModel model;
        model.setBoardForTest(HPSDRHW::HermesLiteRxOnly);
        SetupDialog dialog(&model);
        QWidget* page = dialog.realizePageForTest(QStringLiteral("Two-Tone IMD"));
        QVERIFY(page != nullptr);
        QVERIFY(!page->isEnabled());
        QCOMPARE(page->toolTip(), RadioModel::rxOnlyForcedReason());
        QTreeWidgetItem* transmit = setupRow(dialog, QStringLiteral("Transmit"), /*category=*/true);
        QVERIFY(transmit != nullptr);
        QVERIFY(!transmit->isHidden());
        QCOMPARE(transmit->toolTip(0), RadioModel::rxOnlyForcedReason());

        // Fix wave 2 (Important 2): the PA category is shown on the kit,
        // disabled with the kit's reason, not hidden.
        QTreeWidgetItem* pa = setupRow(dialog, QStringLiteral("PA"), /*category=*/true);
        QVERIFY(pa != nullptr);
        QVERIFY(!pa->isHidden());
        QCOMPARE(pa->toolTip(0), RadioModel::rxOnlyForcedReason());
        auto* notice = dialog.findChild<QLabel*>(QStringLiteral("setupReceiveOnly"));
        QVERIFY(notice != nullptr);
        for (const QString& label : {QStringLiteral("PA Gain"), QStringLiteral("Watt Meter"),
                                     QStringLiteral("PA Values")}) {
            QTreeWidgetItem* leaf = setupRow(dialog, label, /*category=*/false);
            QVERIFY2(leaf != nullptr, qPrintable(label));
            QVERIFY2(!leaf->isHidden(), qPrintable(label));
            QCOMPARE(leaf->toolTip(0), RadioModel::rxOnlyForcedReason());
            dialog.selectPage(label);
            QWidget* paPage = dialog.realizedPageForTest(label);
            QVERIFY2(paPage != nullptr, qPrintable(label));
            QVERIFY2(!paPage->isEnabled(), qPrintable(label));
            QCOMPARE(paPage->toolTip(), RadioModel::rxOnlyForcedReason());
            QVERIFY(paPage->isVisibleTo(&dialog));
            QVERIFY(!notice->isHidden());
            QCOMPARE(notice->text(), RadioModel::rxOnlyForcedReason());
        }
    }

    // Fix wave 2 (Important 2): a radio without power amplifier settings
    // (Atlas), or no radio yet, shows the PA pages disabled with the reason,
    // never hidden; a radio that has them brings them back live.
    void setupPaIsDisabledWithTheReasonWithoutPowerAmplifierSettings_data()
    {
        QTest::addColumn<int>("board");
        QTest::addColumn<QString>("reason");
        QTest::newRow("Atlas") << int(HPSDRHW::Atlas)
                               << QStringLiteral("This radio has no power amplifier settings.");
        QTest::newRow("no radio") << int(HPSDRHW::Unknown)
                                  << QStringLiteral(
                                         "Connect a radio to change its power amplifier settings.");
    }

    void setupPaIsDisabledWithTheReasonWithoutPowerAmplifierSettings()
    {
        QFETCH(int, board);
        QFETCH(QString, reason);
        RadioModel model;
        if (static_cast<HPSDRHW>(board) != HPSDRHW::Unknown) {
            model.setBoardForTest(static_cast<HPSDRHW>(board));
        }
        QVERIFY(!model.boardCapabilities().hasPaProfile);
        QVERIFY(!model.isRxOnly());
        SetupDialog dialog(&model);
        QTreeWidgetItem* pa = setupRow(dialog, QStringLiteral("PA"), /*category=*/true);
        QVERIFY(pa != nullptr);
        QVERIFY(!pa->isHidden());
        QCOMPARE(pa->toolTip(0), reason);
        QVERIFY(OperatorWording::isPlain(reason));
        auto* notice = dialog.findChild<QLabel*>(QStringLiteral("setupNoPowerAmplifier"));
        QVERIFY(notice != nullptr);
        const QStringList labels{QStringLiteral("PA Gain"), QStringLiteral("Watt Meter"),
                                 QStringLiteral("PA Values")};
        for (const QString& label : labels) {
            QTreeWidgetItem* leaf = setupRow(dialog, label, /*category=*/false);
            QVERIFY2(leaf != nullptr, qPrintable(label));
            QVERIFY2(!leaf->isHidden(), qPrintable(label));
            QCOMPARE(leaf->toolTip(0), reason);
            dialog.selectPage(label);
            QWidget* page = dialog.realizedPageForTest(label);
            QVERIFY2(page != nullptr, qPrintable(label));
            QVERIFY2(!page->isEnabled(), qPrintable(label));
            QCOMPARE(page->toolTip(), reason);
            QVERIFY(page->isVisibleTo(&dialog));
            QVERIFY(!notice->isHidden());
            QCOMPARE(notice->text(), reason);
        }

        // A radio with power amplifier settings connects (an ANAN-G2).
        model.setBoardForTest(HPSDRHW::Saturn);
        emit model.currentRadioChanged(RadioInfo{});
        QVERIFY(pa->toolTip(0).isEmpty());
        for (const QString& label : labels) {
            QWidget* page = dialog.realizedPageForTest(label);
            QVERIFY2(page->isEnabled(), qPrintable(label));
            QVERIFY(page->toolTip().isEmpty());
            QVERIFY(setupRow(dialog, label, /*category=*/false)->toolTip(0).isEmpty());
        }
        QVERIFY(notice->isHidden());
    }

    // A remote window: the Core's receive only. Since parity Task 5 the
    // two-tone settings key nothing and no longer wait for remote transmit
    // (the page gates its own controls on transmitSettingsVersion 5), so
    // the page and the Transmit row name receive only alone; the reasons
    // stack where both hold (the TX and Tuner applets, M6). Checkpoint join.
    void setupInARemoteWindowNamesTheReasonsThatHold()
    {
        const QString transmitReason = QStringLiteral("Remote transmit is unavailable.");
        RadioModel window(RadioModel::Role::Remote);
        SetupDialog dialog(&window);
        dialog.setTransmitPermitted(false, transmitReason);
        const QString label = QStringLiteral("Two-Tone IMD");
        dialog.selectPage(label);
        QWidget* page = dialog.realizedPageForTest(label);
        QVERIFY(page != nullptr);
        QVERIFY(page->isEnabled());

        window.applyRxOnlySetting(true);
        const QString reason = window.rxOnlyReason();
        QVERIFY(!page->isEnabled());
        QCOMPARE(page->toolTip(), reason);
        auto* notice = dialog.findChild<QLabel*>(QStringLiteral("setupReceiveOnly"));
        QVERIFY(notice != nullptr);
        QVERIFY(!notice->isHidden());
        QCOMPARE(notice->text(), reason);
        QVERIFY(dialog.findChild<QLabel*>(QStringLiteral("setupTransmitUnavailable"))->isHidden());
        // Fix wave 2 (Minor 1): the Transmit row says what its pages say.
        QTreeWidgetItem* transmit = setupRow(dialog, QStringLiteral("Transmit"), /*category=*/true);
        QVERIFY(transmit != nullptr);
        QCOMPARE(transmit->toolTip(0), reason);

        window.applyRxOnlySetting(false);
        QVERIFY(page->isEnabled());
        QVERIFY(notice->isHidden());
        QVERIFY(transmit->toolTip(0).isEmpty());
    }

    // ---- The TGXL autotune (fix wave M2) -----------------------------------

    void tgxlAutotuneIsRefusedWhileTransmitIsBlocked_data()
    {
        QTest::addColumn<bool>("inhibit");
        QTest::newRow("receive only") << false;
        QTest::newRow("tx inhibit") << true;
    }

    // Refused with the reason before the TGXL connection is even looked at,
    // so nothing reaches the amplifier or the tuner (the PGXL operate=0 and
    // the TGXL autotune come after that check).
    void tgxlAutotuneIsRefusedWhileTransmitIsBlocked()
    {
        QFETCH(bool, inhibit);
        Rig rig;
        if (inhibit) {
            rig.mox()->setTxInhibited(true);
        } else {
            rig.model->setRxOnly(true);
        }
        const QString reason = rig.mox()->transmitBlockReason();
        QVERIFY(!reason.isEmpty());
        QSignalSpy refused(rig.model.get(), &RadioModel::tuneRefused);
        rig.model->startTgxlAutotune(/*fromHardware=*/false);
        rig.model->startTgxlAutotune(/*fromHardware=*/true);
        QCOMPARE(refused.count(), 2);
        QCOMPARE(refused.at(0).at(0).toString(), reason);
        QVERIFY(OperatorWording::isPlain(reason));
        pump();
        QVERIFY(!rig.mox()->isMox());
        QVERIFY(!rig.model->isTune());
    }

    // Fix wave 2 (Minor 3): a TUNE press on the TGXL itself (it reports
    // tuning=1) while transmit is blocked keys nothing and says why.
    void tgxlHardwareTuneWhileBlockedSaysWhy_data()
    {
        QTest::addColumn<bool>("inhibit");
        QTest::newRow("receive only") << false;
        QTest::newRow("tx inhibit") << true;
    }

    void tgxlHardwareTuneWhileBlockedSaysWhy()
    {
        QFETCH(bool, inhibit);
        Rig rig;
        TunerModel* tuner = rig.model->tunerModel();
        QVERIFY(tuner != nullptr);
        TunerApplet applet(rig.model.get(), tuner);
        if (inhibit) {
            rig.mox()->setTxInhibited(true);
        } else {
            rig.model->setRxOnly(true);
        }
        const QString reason = rig.mox()->transmitBlockReason();
        QVERIFY(!reason.isEmpty());
        QSignalSpy refused(rig.model.get(), &RadioModel::tuneRefused);
        tuner->applyStatus({{QStringLiteral("tuning"), QStringLiteral("1")}});
        pump();
        QCOMPARE(refused.count(), 1);
        QCOMPARE(refused.at(0).at(0).toString(), reason);
        QVERIFY(!rig.mox()->isMox());
        QVERIFY(!rig.model->isTune());
        // The sweep ends: nothing of ours to drop.
        tuner->applyStatus({{QStringLiteral("tuning"), QStringLiteral("0")}});
        pump();
        QCOMPARE(refused.count(), 1);
        QVERIFY(!rig.model->isTune());
    }

    void tunerAppletTuneFollowsTheTransmitBlock()
    {
        Rig rig;
        TunerApplet applet(rig.model.get());
        QPushButton* tune = tunerTuneButton(applet);
        QVERIFY(tune != nullptr);
        QVERIFY(tune->isEnabled());

        rig.model->setRxOnly(true);
        QVERIFY(!tune->isEnabled());
        QCOMPARE(tune->toolTip(), rig.model->rxOnlyReason());
        rig.model->setRxOnly(false);
        QVERIFY(tune->isEnabled());
        QVERIFY(tune->toolTip().isEmpty());

        rig.mox()->setTxInhibited(true);
        QVERIFY(!tune->isEnabled());
        QCOMPARE(tune->toolTip(), QStringLiteral("Transmit is inhibited."));
        rig.mox()->setTxInhibited(false);
        QVERIFY(tune->isEnabled());
    }

    void tunerAppletTuneNamesBothReasonsInARemoteWindow()
    {
        const QString remote = QStringLiteral("Remote tuner control is not available yet.");
        RadioModel window(RadioModel::Role::Remote);
        TunerApplet applet(&window);
        applet.setTransmitPermitted(false, remote);
        QPushButton* tune = tunerTuneButton(applet);
        QVERIFY(tune != nullptr);
        QCOMPARE(tune->toolTip(), remote);
        window.applyRxOnlySetting(true);
        QVERIFY(!tune->isEnabled());
        QCOMPARE(tune->toolTip(), window.rxOnlyReason() + QLatin1Char(' ') + remote);
        window.applyRxOnlySetting(false);
        QCOMPARE(tune->toolTip(), remote);
    }

    // ---- A remote window ---------------------------------------------------

    void remoteCheckboxReachesTheCoresGate()
    {
        QTemporaryDir dir;
        AppSettings coreSettings(dir.filePath(QStringLiteral("station.settings")));
        Rig core;
        {
            StationServer server(core.model.get(), coreSettings,
                                 NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
            RadioModel window(RadioModel::Role::Remote);
            SettingsProxy proxy;
            StationClient client(&window, &proxy);
            auto* stationEnd = new LoopbackTransport(QStringLiteral("station-end"), this);
            auto* clientEnd = new LoopbackTransport(QStringLiteral("client-end"), this);
            stationEnd->linkTo(clientEnd);
            QSignalSpy completed(&client, &StationClient::handshakeComplete);
            client.startSession(clientEnd, server.token());
            server.acceptTransport(stationEnd);
            QVERIFY(completed.wait(5000) || !completed.isEmpty());

            ScopedRemoteBackend backend(&proxy);
            GeneralOptionsPage page(&window);
            page.setEnableTransmitConfirmForTest([] { return true; });
            QCheckBox* box = rxOnlyBox(page);
            QVERIFY(box != nullptr);
            QVERIFY(!box->isHidden());
            QVERIFY(!box->isChecked());

            box->click();
            QTRY_COMPARE(coreSettings.value(kKey).toString(), QStringLiteral("True"));
            QTRY_VERIFY(core.model->isRxOnly());
            QVERIFY(core.mox()->isRxOnly());
            QVERIFY(window.isRxOnly());

            // The Core's gate refuses its own sources.
            core.mox()->onCatPtt(true);
            core.model->setMoxFromButton(true);
            pump();
            QVERIFY(!core.mox()->isMox());

            box->click();   // off, answered Yes
            QTRY_COMPARE(coreSettings.value(kKey).toString(), QStringLiteral("False"));
            QTRY_VERIFY(!core.model->isRxOnly());

            // A change made on the Core reaches the window and its box.
            coreSettings.setValue(kKey, QStringLiteral("True"));
            QTRY_VERIFY(core.model->isRxOnly());
            QTRY_VERIFY(window.isRxOnly());
            QTRY_VERIFY(box->isChecked());

            // A settings reset on the Core reads the default (off).
            coreSettings.remove(kKey);
            QTRY_VERIFY(!core.model->isRxOnly());
            QTRY_VERIFY(!window.isRxOnly());
        }
    }

    void remoteWindowOnAKitCoreShowsReceiveOnly()
    {
        RadioModel window(RadioModel::Role::Remote);
        StationCapabilities caps;
        caps.macAddress = QStringLiteral("AA:BB:CC:DD:EE:16");
        caps.board = HPSDRHW::HermesLiteRxOnly;
        caps.radioConnected = true;
        caps.radioIdentityEntries = true;
        caps.hpsdrModel = HPSDRModel::HERMESLITE;
        caps.radioProtocol = 1;
        window.applyStationCapabilities(caps);
        QVERIFY(window.isRxOnly());
        QVERIFY(window.isRxOnlyForced());

        GeneralOptionsPage page(&window);
        QCheckBox* box = rxOnlyBox(page);
        QVERIFY(box != nullptr);
        QVERIFY(box->isChecked());
        QVERIFY(!box->isEnabled());
        QCOMPARE(box->toolTip(), RadioModel::rxOnlyForcedReason());

        // The Core's gate on a remote window: still locked while the
        // window has no Core settings, and unlocked to the gate's state.
        page.setStationSettingsAvailable(false, QStringLiteral("Connect to the Core to change these."));
        QVERIFY(!box->isEnabled());
        page.setStationSettingsAvailable(true, QString());
        QVERIFY(!box->isEnabled());
        QCOMPARE(box->toolTip(), RadioModel::rxOnlyForcedReason());
    }

    void remoteCheckboxIsDisabledWithoutTheCoresSettings()
    {
        RadioModel window(RadioModel::Role::Remote);
        GeneralOptionsPage page(&window);
        QCheckBox* box = rxOnlyBox(page);
        QVERIFY(box != nullptr);
        const QString reason = QStringLiteral("Connect to the Core to change these.");
        page.setStationSettingsAvailable(false, reason);
        QVERIFY(!box->isEnabled());
        QCOMPARE(box->toolTip(), reason);
        page.setStationSettingsAvailable(true, QString());
        QVERIFY(box->isEnabled());
    }

    void olderCoreRefusalPutsTheBoxBackAndSaysSo()
    {
        RadioModel window(RadioModel::Role::Remote);
        SettingsProxy proxy;
        ScopedRemoteBackend backend(&proxy);
        GeneralOptionsPage page(&window);
        QCheckBox* box = rxOnlyBox(page);
        auto* note = page.findChild<QLabel*>(QStringLiteral("lblRxOnlyCore"));
        QVERIFY(box != nullptr && note != nullptr);
        QVERIFY(note->isHidden());

        box->click();
        QVERIFY(window.isRxOnly());
        // An older Core keeps the key to itself and has no value for it.
        proxy.applyRejection(kKey, QVariant());
        QVERIFY(!box->isChecked());
        QVERIFY(!window.isRxOnly());
        QVERIFY(!note->isHidden());
        QVERIFY(OperatorWording::isPlain(note->text()));
        QVERIFY(note->text().contains(QStringLiteral("Core needs updating")));
    }

    // ---- Wording -----------------------------------------------------------

    void everyReasonIsPlain()
    {
        QVERIFY(OperatorWording::isPlain(MoxController::defaultRxOnlyReason()));
        QVERIFY(OperatorWording::isPlain(RadioModel::rxOnlyForcedReason()));
        QVERIFY(OperatorWording::coreCalledStationIn(MoxController::defaultRxOnlyReason()).isEmpty());
        QVERIFY(OperatorWording::coreCalledStationIn(RadioModel::rxOnlyForcedReason()).isEmpty());
    }
};

QTEST_MAIN(TestReceiveOnly)
#include "tst_receive_only.moc"
