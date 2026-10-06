// =================================================================
// tests/tst_mox_controller_ptt_sources.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original test. No Thetis logic is ported in this file; it
// checks that the keying sources follow Thetis (receiver and transmit
// gaps plan, Task 7):
//
//   Thetis Project Files/Source/Console/console.cs [v2.10.3.15]:
//     PollPTT                              25463-25623
//       the whole pass is skipped while _manual_mox is set    25470
//       keying from receive, in order TCI, CAT, CW, MIC, VOX  25507-25555
//       release only in the mode the source set               25558-25608
//     getFallbackPTTModeAfterTCIRelease    25429-25461
//     chkMOX_CheckedChanged2, TX-to-RX branch:
//       CATPTT / TCIPTT cleared            29406-29411
//       _current_ptt_mode = PTTMode.NONE   29547
//     chkMOX_Click (the MOX button)        29730-29747
//     chkTUN_CheckedChanged: MANUAL + _manual_mox at 30144-30145,
//       _manual_mox cleared at the end of TUN-off 30193
//   setup.cs [v2.10.3.15]: two-tone sets console.ManualMox around its key
//     (11162, 11193).
//
// Every case runs a bare MoxController (or an unconnected RadioModel)
// with 0 ms walk timers. Nothing here opens an audio device or keys a
// radio.
// =================================================================

// no-port-check: NereusSDR-original test file, no upstream Thetis port.

#include <QtTest/QtTest>
#include <QSignalSpy>

#include "core/MoxController.h"
#include "core/PttMode.h"
#include "core/TxInterlockPolicy.h"
#include "core/WdspTypes.h"
#include "models/RadioModel.h"

using namespace NereusSDR;

namespace {

void makeSync(MoxController& ctrl)
{
    ctrl.setTimerIntervals(0, 0, 0, 0, 0, 0);
}

void drain()
{
    for (int i = 0; i < 4; ++i) {
        QCoreApplication::processEvents();
    }
}

// The radio's status frames report the mic PTT level on every frame; a few
// frames stand in for "the operator is not pressing the mic".
void micFrames(MoxController& ctrl, bool pressed, int frames = 3)
{
    for (int i = 0; i < frames; ++i) {
        ctrl.onMicPttFromRadio(pressed);
        drain();
    }
}

} // namespace

class TestMoxControllerPttSources : public QObject {
    Q_OBJECT

private slots:

    // ── Each source's PTT mode, and the mode clearing on unkey ──────────────

    void mic_keysInMicMode_unkeyClearsMode()
    {
        MoxController ctrl;
        makeSync(ctrl);
        micFrames(ctrl, true);
        QVERIFY(ctrl.isMox());
        QCOMPARE(ctrl.pttMode(), PttMode::Mic);

        micFrames(ctrl, false);
        QVERIFY(!ctrl.isMox());
        // chkMOX_CheckedChanged2 sets PTTMode.NONE on every unkey.
        QCOMPARE(ctrl.pttMode(), PttMode::None);
    }

    void vox_keysInVoxMode_unkeyClearsMode()
    {
        MoxController ctrl;
        makeSync(ctrl);
        ctrl.onVoxActive(true);
        drain();
        QVERIFY(ctrl.isMox());
        QCOMPARE(ctrl.pttMode(), PttMode::Vox);

        ctrl.onVoxActive(false);
        drain();
        QVERIFY(!ctrl.isMox());
        QCOMPARE(ctrl.pttMode(), PttMode::None);
    }

    void vox_doesNotKeyOutsideVoiceModes()
    {
        // PollPTT keys VOX only in LSB/USB/DSB/AM/SAM/DIGU/DIGL/FM.
        MoxController ctrl;
        makeSync(ctrl);
        ctrl.onModeChanged(DSPMode::CWU);
        ctrl.onVoxActive(true);
        drain();
        QVERIFY(!ctrl.isMox());
        QCOMPARE(ctrl.pttMode(), PttMode::None);
    }

    void tci_keysInTciMode_releaseUnkeysAndClearsMode()
    {
        MoxController ctrl;
        makeSync(ctrl);
        ctrl.onTciPtt(true);
        drain();
        QVERIFY(ctrl.isMox());
        QCOMPARE(ctrl.pttMode(), PttMode::Tci);

        ctrl.onTciPtt(false);
        drain();
        QVERIFY(!ctrl.isMox());
        QCOMPARE(ctrl.pttMode(), PttMode::None);
    }

    void moxButton_isAManualKey_withNoPttMode()
    {
        // chkMOX_Click sets _manual_mox; the MOX button sets no PTT mode
        // (only TUN sets PTTMode.MANUAL).
        MoxController ctrl;
        makeSync(ctrl);
        ctrl.onMoxButton(true);
        drain();
        QVERIFY(ctrl.isMox());
        QVERIFY(ctrl.isManualKey());
        QVERIFY(!ctrl.isManualMox());   // the TUN button's flag stays off
        QCOMPARE(ctrl.pttMode(), PttMode::None);

        ctrl.onMoxButton(false);
        drain();
        QVERIFY(!ctrl.isMox());
        QVERIFY(!ctrl.isManualKey());
        QCOMPARE(ctrl.pttMode(), PttMode::None);
    }

    void moxButton_refusedKey_leavesNoManualKey()
    {
        // A refused key unchecks chkMOX; chkMOX_Click then clears _manual_mox.
        MoxController ctrl;
        makeSync(ctrl);
        ctrl.setMoxCheck([]() {
            return safety::BandPlanGuard::MoxCheckResult{false,
                QStringLiteral("test refusal")};
        });
        ctrl.onMoxButton(true);
        drain();
        QVERIFY(!ctrl.isMox());
        QVERIFY(!ctrl.isManualKey());
    }

    void refusedKey_leavesModeNone()
    {
        // Thetis refuses a key by unchecking chkMOX, which runs the TX-to-RX
        // branch: the mode is NONE afterwards.
        MoxController ctrl;
        makeSync(ctrl);
        ctrl.setMoxCheck([]() {
            return safety::BandPlanGuard::MoxCheckResult{false,
                QStringLiteral("test refusal")};
        });
        ctrl.onMicPttFromRadio(true);
        drain();
        QVERIFY(!ctrl.isMox());
        QCOMPARE(ctrl.pttMode(), PttMode::None);
    }

    void tune_isAManualKey_heldUntilTuneOffCompletes()
    {
        MoxController ctrl;
        makeSync(ctrl);
        ctrl.setTune(true);
        drain();
        QVERIFY(ctrl.isMox());
        QVERIFY(ctrl.isManualKey());
        QCOMPARE(ctrl.pttMode(), PttMode::Manual);

        // A mic PTT pressed while the tune tone is still being taken down
        // must not key (_manual_mox is cleared only at the end of TUN-off).
        ctrl.setTune(false);
        drain();
        QVERIFY(!ctrl.isMox());
        QCOMPARE(ctrl.pttMode(), PttMode::None);
        QVERIFY(ctrl.isManualKey());
        micFrames(ctrl, true);
        QVERIFY(!ctrl.isMox());

        // TUN-off completes: the held mic keys on the next pass.
        ctrl.setManualKey(false);
        drain();
        QVERIFY(ctrl.isMox());
        QCOMPARE(ctrl.pttMode(), PttMode::Mic);
    }

    // ── Release guards, one per pair ────────────────────────────────────────

    void micRelease_duringManualKey_keepsTransmitting()
    {
        MoxController ctrl;
        makeSync(ctrl);
        // The operator used the mic earlier in the session.
        micFrames(ctrl, true);
        micFrames(ctrl, false);
        QVERIFY(!ctrl.isMox());

        ctrl.onMoxButton(true);
        drain();
        QVERIFY(ctrl.isMox());

        // Every status frame reports the mic up; PollPTT does nothing while
        // _manual_mox is set.
        micFrames(ctrl, false, 5);
        QVERIFY(ctrl.isMox());
        QCOMPARE(ctrl.pttMode(), PttMode::None);

        // Nor does a mic press take the mode.
        micFrames(ctrl, true);
        micFrames(ctrl, false);
        QVERIFY(ctrl.isMox());
        QCOMPARE(ctrl.pttMode(), PttMode::None);
    }

    void voxRelease_duringManualKey_keepsTransmitting()
    {
        MoxController ctrl;
        makeSync(ctrl);
        ctrl.onMoxButton(true);
        drain();
        QVERIFY(ctrl.isMox());

        ctrl.onVoxActive(true);
        drain();
        QCOMPARE(ctrl.pttMode(), PttMode::None);
        ctrl.onVoxActive(false);
        drain();
        QVERIFY(ctrl.isMox());
        QVERIFY(ctrl.isManualKey());
    }

    void micRelease_duringTwoToneManualKey_keepsTransmitting()
    {
        // Two-tone: console.ManualMox = true; console.MOX = true.
        MoxController ctrl;
        makeSync(ctrl);
        micFrames(ctrl, true);
        micFrames(ctrl, false);

        ctrl.setManualKey(true);
        ctrl.setMox(true);
        drain();
        micFrames(ctrl, false);
        QVERIFY(ctrl.isMox());
    }

    void voxRelease_duringMicKey_keepsTransmitting()
    {
        // PollPTT's VOX release applies only in PTTMode.VOX.
        MoxController ctrl;
        makeSync(ctrl);
        ctrl.onMicPttFromRadio(true);
        drain();
        ctrl.onVoxActive(true);
        drain();
        QCOMPARE(ctrl.pttMode(), PttMode::Mic);
        ctrl.onVoxActive(false);
        ctrl.onMicPttFromRadio(true);
        drain();
        QVERIFY(ctrl.isMox());
        QCOMPARE(ctrl.pttMode(), PttMode::Mic);
    }

    void micRelease_duringVoxKey_keepsTransmitting()
    {
        MoxController ctrl;
        makeSync(ctrl);
        ctrl.onVoxActive(true);
        drain();
        micFrames(ctrl, false);
        QVERIFY(ctrl.isMox());
        QCOMPARE(ctrl.pttMode(), PttMode::Vox);
    }

    void catPress_duringMicKey_leavesMicMode()
    {
        // Keying happens only from receive; a second source does not take
        // the mode.
        MoxController ctrl;
        makeSync(ctrl);
        ctrl.onMicPttFromRadio(true);
        drain();
        ctrl.onCatPtt(true);
        drain();
        QCOMPARE(ctrl.pttMode(), PttMode::Mic);
        ctrl.onCatPtt(false);
        ctrl.onMicPttFromRadio(true);
        drain();
        QVERIFY(ctrl.isMox());
        QCOMPARE(ctrl.pttMode(), PttMode::Mic);
    }

    void tciPressAndRelease_duringManualKey_doNothing()
    {
        MoxController ctrl;
        makeSync(ctrl);
        ctrl.onMoxButton(true);
        drain();
        ctrl.onTciPtt(true);
        drain();
        QCOMPARE(ctrl.pttMode(), PttMode::None);
        ctrl.onTciPtt(false);
        drain();
        QVERIFY(ctrl.isMox());
        QVERIFY(ctrl.isManualKey());
    }

    void tciRelease_withMicHeld_fallsBackToMic()
    {
        // getFallbackPTTModeAfterTCIRelease: a held mic keeps the key.
        MoxController ctrl;
        makeSync(ctrl);
        ctrl.onTciPtt(true);
        drain();
        QCOMPARE(ctrl.pttMode(), PttMode::Tci);
        ctrl.onMicPttFromRadio(true);
        drain();
        QCOMPARE(ctrl.pttMode(), PttMode::Tci);

        ctrl.onTciPtt(false);
        drain();
        QVERIFY(ctrl.isMox());
        QCOMPARE(ctrl.pttMode(), PttMode::Mic);

        micFrames(ctrl, false);
        QVERIFY(!ctrl.isMox());
        QCOMPARE(ctrl.pttMode(), PttMode::None);
    }

    void heldVox_keysAfterManualUnkey()
    {
        // After the MOX button goes off, Thetis's next poll keys a VOX that
        // is still active.
        MoxController ctrl;
        makeSync(ctrl);
        ctrl.onMoxButton(true);
        drain();
        ctrl.onVoxActive(true);
        drain();
        ctrl.onMoxButton(false);
        drain();
        QVERIFY(ctrl.isMox());
        QCOMPARE(ctrl.pttMode(), PttMode::Vox);
        QVERIFY(!ctrl.isManualKey());
    }

    // ── Task 7 fix wave, I2: TX inhibit and the PA trip ─────────────────────
    //
    // PollPTT's gate (console.cs:25470 [v2.10.3.15]) skips every source
    // while _tx_inhibit or _ganymede_pa_issue is set; chkMOX_CheckedChanged2
    // aborts any key while the PA is tripped (console.cs:29364-29371), the
    // TXInhibit setter disables MOX, TUN, two-tone and VOX and unkeys
    // (console.cs:15341-15363), and a trip unkeys (Andromeda.cs:944-945).
    // One case per source, for each gate.

    void blockedSources_data()
    {
        QTest::addColumn<bool>("paTrip");
        QTest::addColumn<QString>("source");
        const QStringList sources{QStringLiteral("mic"), QStringLiteral("vox"),
                                  QStringLiteral("cat"), QStringLiteral("tci"),
                                  QStringLiteral("mox button"), QStringLiteral("tun"),
                                  QStringLiteral("two-tone")};
        for (const bool paTrip : {false, true}) {
            for (const QString& source : sources) {
                const QString row = (paTrip ? QStringLiteral("pa trip, ")
                                            : QStringLiteral("tx inhibit, ")) + source;
                QTest::newRow(qPrintable(row)) << paTrip << source;
            }
        }
    }

    void blockedSources()
    {
        QFETCH(bool, paTrip);
        QFETCH(QString, source);
        MoxController ctrl;
        makeSync(ctrl);
        if (paTrip) { ctrl.setPaTripped(true); } else { ctrl.setTxInhibited(true); }
        QSignalSpy rejected(&ctrl, &MoxController::moxRejected);

        if (source == QLatin1String("mic")) {
            micFrames(ctrl, true);
        } else if (source == QLatin1String("vox")) {
            ctrl.onVoxActive(true);
        } else if (source == QLatin1String("cat")) {
            ctrl.onCatPtt(true);
        } else if (source == QLatin1String("tci")) {
            ctrl.onTciPtt(true);
        } else if (source == QLatin1String("mox button")) {
            ctrl.onMoxButton(true);
        } else if (source == QLatin1String("tun")) {
            ctrl.setTune(true);
        } else {
            // Two-tone keys with the manual key and setMox(true).
            ctrl.setManualKey(true);
            ctrl.setMox(true);
        }
        drain();
        QVERIFY2(!ctrl.isMox(), qPrintable(source + QStringLiteral(" keyed while blocked")));

        if (source == QLatin1String("mox button")) {
            // A refused press leaves the button off (chkMOX_Click's else
            // branch) and tells the operator why.
            QVERIFY(!ctrl.isManualKey());
            QCOMPARE(rejected.count(), 1);
            QVERIFY(!rejected.first().at(0).toString().isEmpty());
        }
        if (source == QLatin1String("mic") || source == QLatin1String("vox")
            || source == QLatin1String("cat") || source == QLatin1String("tci")) {
            // PollPTT's gate skips the pass: no refusal toast per status frame.
            QCOMPARE(rejected.count(), 0);
            QCOMPARE(ctrl.pttMode(), PttMode::None);
        }
    }

    void blockUnkeysActiveTransmission_data()
    {
        QTest::addColumn<bool>("paTrip");
        QTest::newRow("tx inhibit") << false;
        QTest::newRow("pa trip") << true;
    }

    void blockUnkeysActiveTransmission()
    {
        QFETCH(bool, paTrip);
        const auto block = [paTrip](MoxController& c, bool on) {
            if (paTrip) { c.setPaTripped(on); } else { c.setTxInhibited(on); }
        };

        {   // a MOX-button key
            MoxController ctrl; makeSync(ctrl);
            ctrl.onMoxButton(true); drain();
            QVERIFY(ctrl.isMox());
            block(ctrl, true); drain();
            QVERIFY2(!ctrl.isMox(), "a MOX-button key survived the block");
        }
        {   // a mic key: the held mic does not key again while blocked
            MoxController ctrl; makeSync(ctrl);
            micFrames(ctrl, true);
            QVERIFY(ctrl.isMox());
            block(ctrl, true); drain();
            QVERIFY2(!ctrl.isMox(), "a mic key survived the block");
            micFrames(ctrl, true);
            QVERIFY(!ctrl.isMox());
            // Cleared: the next pass keys the mic still held, as Thetis's
            // next poll does.
            block(ctrl, false);
            micFrames(ctrl, true);
            QVERIFY(ctrl.isMox());
            QCOMPARE(ctrl.pttMode(), PttMode::Mic);
        }
    }

    // ── Task 7 follow-up, N3: CAT and TCI under a block are dropped ─────────
    // A CAT or TCI request is an app's, not a held switch: made while TX
    // inhibit or a PA trip holds, it is refused (the app is answered
    // trx:0,false) and not kept, so it does not key when the block lifts.
    // A level recorded before the block is dropped when the block is
    // asserted. The mic and VOX keep Thetis's behaviour (a person is
    // holding them; blockUnkeysActiveTransmission).
    void catTciUnderBlock_areDroppedNotHeld_data()
    {
        QTest::addColumn<bool>("paTrip");
        QTest::addColumn<bool>("cat");
        QTest::newRow("tx inhibit, tci") << false << false;
        QTest::newRow("tx inhibit, cat") << false << true;
        QTest::newRow("pa trip, tci")    << true  << false;
        QTest::newRow("pa trip, cat")    << true  << true;
    }
    void catTciUnderBlock_areDroppedNotHeld()
    {
        QFETCH(bool, paTrip);
        QFETCH(bool, cat);
        const auto block = [paTrip](MoxController& c, bool on) {
            if (paTrip) { c.setPaTripped(on); } else { c.setTxInhibited(on); }
        };
        const auto press = [cat](MoxController& c) {
            if (cat) { c.onCatPtt(true); } else { c.onTciPtt(true); }
            drain();
        };

        {   // requested while the block holds
            MoxController ctrl; makeSync(ctrl);
            block(ctrl, true);
            press(ctrl);
            QVERIFY(!ctrl.isMox());
            block(ctrl, false);
            micFrames(ctrl, false);   // the next PollPTT passes
            QVERIFY2(!ctrl.isMox(), "a request made under the block keyed when it lifted");
            QCOMPARE(ctrl.pttMode(), PttMode::None);
        }
        {   // requested before the block, held off by a manual key
            MoxController ctrl; makeSync(ctrl);
            ctrl.setManualKey(true);
            press(ctrl);
            QVERIFY(!ctrl.isMox());
            block(ctrl, true);
            block(ctrl, false);
            ctrl.setManualKey(false);
            drain();
            QVERIFY2(!ctrl.isMox(), "a request held across the block keyed when it lifted");
        }
        {   // a new request after the block lifts keys normally
            MoxController ctrl; makeSync(ctrl);
            block(ctrl, true);
            press(ctrl);
            block(ctrl, false);
            press(ctrl);
            QVERIFY(ctrl.isMox());
            QCOMPARE(ctrl.pttMode(), cat ? PttMode::Cat : PttMode::Tci);
        }
    }

    void trxUnderInhibit_viaRadioModel_isAnsweredFalseAndDropped()
    {
        RadioModel core;
        MoxController* mox = core.moxController();
        QVERIFY(mox != nullptr);
        makeSync(*mox);
        mox->setMoxCheck([]() {
            return safety::BandPlanGuard::MoxCheckResult{true, QString()};
        });
        mox->setTxInhibited(true);
        core.setMox(true);
        drain();
        QVERIFY(!core.mox());   // what TciProtocol answers: trx:0,false
        mox->setTxInhibited(false);
        micFrames(*mox, false);
        QVERIFY2(!core.mox(), "the trx made under the inhibit keyed when it lifted");
    }

    // ── Task 7 fix wave, M2: a TX-interlock refusal is reported ────────────
    // TwoToneController, TUN and the MOX button learn of a refused key
    // through moxRejected; the interlock refused without it.
    void interlockRefusal_emitsMoxRejected()
    {
        TxInterlockPolicy policy;
        policy.setMode(TxInterlockPolicy::Block);
        MoxController ctrl;
        makeSync(ctrl);
        ctrl.setInterlockPolicy(&policy);
        ctrl.onAmpStateChanged(/*hasAmp=*/true, /*inOperate=*/false);
        QSignalSpy rejected(&ctrl, &MoxController::moxRejected);
        QSignalSpy denied(&policy, &TxInterlockPolicy::denied);

        ctrl.onMoxButton(true);
        drain();
        QVERIFY(!ctrl.isMox());
        QVERIFY(!ctrl.isManualKey());
        QCOMPARE(denied.count(), 1);
        QCOMPARE(rejected.count(), 1);
        QVERIFY(rejected.first().at(0).toString().contains(
            denied.first().at(0).toString()));
        ctrl.setInterlockPolicy(nullptr);
        policy.setMode(TxInterlockPolicy::Disabled);
    }

    // ── Task 7 fix wave, M3: one refusal message per press ──────────────────
    // A held source is refused on every status frame, but the operator is
    // told once per press. R-R3-36 (Task 7 follow-up): a refusal because
    // the microphone is not ready is never queued, so the held source does
    // not key when the microphone becomes ready; the operator presses again.
    void heldRefusedSource_isReportedOncePerPress()
    {
        MoxController ctrl;
        makeSync(ctrl);
        bool allow = false;
        ctrl.setMoxCheck([&allow]() {
            return safety::BandPlanGuard::MoxCheckResult{allow,
                allow ? QString() : QStringLiteral("Microphone is not ready."),
                /*notQueued=*/!allow};
        });
        QSignalSpy rejected(&ctrl, &MoxController::moxRejected);

        micFrames(ctrl, true, 6);
        QVERIFY(!ctrl.isMox());
        QCOMPARE(rejected.count(), 1);
        micFrames(ctrl, false, 2);
        micFrames(ctrl, true, 4);
        QCOMPARE(rejected.count(), 2);   // a new press is told again
        micFrames(ctrl, false, 2);

        // VOX: the pass runs on every mic frame while VOX stays active.
        ctrl.onVoxActive(true);
        micFrames(ctrl, false, 6);
        QVERIFY(!ctrl.isMox());
        QCOMPARE(rejected.count(), 3);
        ctrl.onVoxActive(false);
        drain();

        // Never queued: the microphone becoming ready does not key the mic
        // still held; a new press does.
        micFrames(ctrl, true, 3);
        QCOMPARE(rejected.count(), 4);
        allow = true;
        micFrames(ctrl, true, 3);
        QVERIFY2(!ctrl.isMox(), "a held mic keyed without a new press");
        micFrames(ctrl, false, 1);
        micFrames(ctrl, true, 1);
        QVERIFY(ctrl.isMox());
        QCOMPARE(ctrl.pttMode(), PttMode::Mic);
        QCOMPARE(rejected.count(), 4);
        micFrames(ctrl, false, 1);
        QVERIFY(!ctrl.isMox());

        // VOX the same way.
        allow = false;
        ctrl.onVoxActive(true);
        drain();
        QCOMPARE(rejected.count(), 5);
        allow = true;
        micFrames(ctrl, false, 3);
        QVERIFY2(!ctrl.isMox(), "a held VOX keyed without a new press");
        ctrl.onVoxActive(false);
        ctrl.onVoxActive(true);
        drain();
        QVERIFY(ctrl.isMox());
        QCOMPARE(ctrl.pttMode(), PttMode::Vox);
    }

    // Thetis's retry stays for the other refusals: PollPTT tries a held
    // source on every pass, so a band-plan (or interlock) refusal keys the
    // held source once it is allowed.
    void heldBandPlanRefusal_keysOnceAllowed()
    {
        MoxController ctrl;
        makeSync(ctrl);
        bool allow = false;
        ctrl.setMoxCheck([&allow]() {
            return safety::BandPlanGuard::MoxCheckResult{allow,
                allow ? QString() : QStringLiteral("Out of band.")};
        });
        QSignalSpy rejected(&ctrl, &MoxController::moxRejected);
        micFrames(ctrl, true, 3);
        QVERIFY(!ctrl.isMox());
        QCOMPARE(rejected.count(), 1);
        allow = true;
        micFrames(ctrl, true, 1);
        QVERIFY(ctrl.isMox());
        QCOMPARE(ctrl.pttMode(), PttMode::Mic);
    }

    // A TCI or CAT refusal drops its level, so the app's next request is a
    // new press; a microphone refusal does not hold it off.
    void refusedTciForMicrophone_nextTrxIsANewPress()
    {
        MoxController ctrl;
        makeSync(ctrl);
        bool allow = false;
        ctrl.setMoxCheck([&allow]() {
            return safety::BandPlanGuard::MoxCheckResult{allow,
                allow ? QString() : QStringLiteral("Microphone is not ready."),
                /*notQueued=*/!allow};
        });
        ctrl.onTciPtt(true);
        drain();
        QVERIFY(!ctrl.isMox());
        allow = true;
        ctrl.onTciPtt(true);
        drain();
        QVERIFY(ctrl.isMox());
        QCOMPARE(ctrl.pttMode(), PttMode::Tci);
    }

    void heldSourceUnderInterlock_isReportedOncePerPress()
    {
        TxInterlockPolicy policy;
        policy.setMode(TxInterlockPolicy::Block);
        MoxController ctrl;
        makeSync(ctrl);
        ctrl.setInterlockPolicy(&policy);
        ctrl.onAmpStateChanged(/*hasAmp=*/true, /*inOperate=*/false);
        QSignalSpy denied(&policy, &TxInterlockPolicy::denied);
        QSignalSpy rejected(&ctrl, &MoxController::moxRejected);

        micFrames(ctrl, true, 6);
        QVERIFY(!ctrl.isMox());
        QCOMPARE(denied.count(), 1);
        QCOMPARE(rejected.count(), 1);
        ctrl.setInterlockPolicy(nullptr);
        policy.setMode(TxInterlockPolicy::Disabled);
    }

    // ── Task 7 follow-up, N2: a CAT or TCI request after a refusal ──────────
    // A refusal drops the CAT and TCI levels, so an app's next request is a
    // new press even without a release in between: the operator is told
    // again.
    void repeatedRefusedTciOrCat_isReportedEachTime_data()
    {
        QTest::addColumn<bool>("cat");
        QTest::newRow("tci") << false;
        QTest::newRow("cat") << true;
    }
    void repeatedRefusedTciOrCat_isReportedEachTime()
    {
        QFETCH(bool, cat);
        MoxController ctrl;
        makeSync(ctrl);
        ctrl.setMoxCheck([]() {
            return safety::BandPlanGuard::MoxCheckResult{false, QStringLiteral("Out of band.")};
        });
        QSignalSpy rejected(&ctrl, &MoxController::moxRejected);
        const auto press = [&ctrl, cat]() {
            if (cat) { ctrl.onCatPtt(true); } else { ctrl.onTciPtt(true); }
            drain();
        };
        press();
        QVERIFY(!ctrl.isMox());
        QCOMPARE(rejected.count(), 1);
        press();
        QVERIFY(!ctrl.isMox());
        QCOMPARE(rejected.count(), 2);
        // Held mic frames do not re-report it: its level was dropped.
        micFrames(ctrl, false);
        QCOMPARE(rejected.count(), 2);
    }

    void repeatedRefusedTrx_viaRadioModel_isReportedEachTime()
    {
        RadioModel core;
        MoxController* mox = core.moxController();
        QVERIFY(mox != nullptr);
        makeSync(*mox);
        mox->setMoxCheck([]() {
            return safety::BandPlanGuard::MoxCheckResult{false, QStringLiteral("Out of band.")};
        });
        QSignalSpy rejected(mox, &MoxController::moxRejected);
        core.setMox(true);
        drain();
        core.setMox(true);
        drain();
        QVERIFY(!core.mox());
        QCOMPARE(rejected.count(), 2);
    }

    // ── Task 7 fix wave, M5: a VOX level cannot outlive a mode change ───────
    // DEXP pushes no pushvox(0) once VOX stops running (dexp.c:328-339), so
    // a switch to a non-voice mode leaves Audio.VOXActive stale. A return to
    // a voice mode must not key from it before DEXP's next push.
    void voxKey_releasedOnSwitchToNonVoiceMode_andNotRekeyed()
    {
        MoxController ctrl;
        makeSync(ctrl);
        ctrl.onModeChanged(DSPMode::USB);
        ctrl.setVoxEnabled(true);
        ctrl.onVoxActive(true);
        drain();
        QVERIFY(ctrl.isMox());
        QCOMPARE(ctrl.pttMode(), PttMode::Vox);

        ctrl.onModeChanged(DSPMode::CWU);
        drain();
        QVERIFY2(!ctrl.isMox(), "a VOX key outlived the switch to CW");

        ctrl.onModeChanged(DSPMode::USB);
        micFrames(ctrl, false);
        QVERIFY2(!ctrl.isMox(), "a stale VOX level keyed on the return to USB");
    }

    void staleVoxLevel_doesNotKeyOnReturnToVoiceMode()
    {
        MoxController ctrl;
        makeSync(ctrl);
        ctrl.onModeChanged(DSPMode::USB);
        ctrl.setVoxEnabled(true);
        // VOX goes active under a manual key (TUN), so it keys nothing.
        ctrl.setManualKey(true);
        ctrl.onVoxActive(true);
        drain();
        // TUN-off restores CW, then clears the manual key.
        ctrl.onModeChanged(DSPMode::CWU);
        ctrl.setManualKey(false);
        drain();
        QVERIFY(!ctrl.isMox());

        ctrl.onModeChanged(DSPMode::USB);
        micFrames(ctrl, false);
        QVERIFY2(!ctrl.isMox(), "a stale VOX level keyed on the return to USB");
    }

    // ── Task 7 fix wave, M6: a TCI release falling back to the mic ──────────
    // The key moves to the mic, which reads the PC microphone; it is admitted
    // like a new key (R-R3-36's ready check sits in the MOX pre-check), so a
    // microphone that is not ready ends the key instead.
    void tciReleaseFallback_runsTheAdmissionCheck()
    {
        MoxController ctrl;
        makeSync(ctrl);
        bool micReady = true;
        int checks = 0;
        ctrl.setMoxCheck([&micReady, &checks]() {
            ++checks;
            return safety::BandPlanGuard::MoxCheckResult{micReady,
                micReady ? QString() : QStringLiteral("Microphone is not ready.")};
        });
        QSignalSpy rejected(&ctrl, &MoxController::moxRejected);
        ctrl.onTciPtt(true);
        drain();
        QVERIFY(ctrl.isMox());
        ctrl.onMicPttFromRadio(true);
        drain();
        QCOMPARE(ctrl.pttMode(), PttMode::Tci);
        const int checksBefore = checks;

        micReady = false;
        ctrl.onTciPtt(false);
        drain();
        QVERIFY(checks > checksBefore);
        QVERIFY2(!ctrl.isMox(), "the mic kept the key with the microphone not ready");
        QCOMPARE(ctrl.pttMode(), PttMode::None);
        QCOMPARE(rejected.count(), 1);
        // The mic, still held, is refused again on each frame but told once.
        micFrames(ctrl, true);
        QVERIFY(!ctrl.isMox());
        QCOMPARE(rejected.count(), 1);
    }

    // ── RadioModel shims (unconnected model, 0 ms walk, counting check) ─────

    void radioModel_tciShim_keysWithTciMode()
    {
        RadioModel core;
        MoxController* mox = core.moxController();
        QVERIFY(mox != nullptr);
        makeSync(*mox);
        int keyRequests = 0;
        mox->setMoxCheck([&keyRequests]() {
            ++keyRequests;
            return safety::BandPlanGuard::MoxCheckResult{true, QString()};
        });

        core.setMox(true);
        drain();
        QVERIFY(core.mox());
        QCOMPARE(mox->pttMode(), PttMode::Tci);
        QCOMPARE(keyRequests, 1);

        // CAT ownership observes an explicit accepted TCI repeat even without a MOX edge.
        const quint64 stamp = mox->acceptedRequestGeneration();
        core.setMox(true);
        drain();
        QCOMPARE(keyRequests, 2);
        QVERIFY(mox->acceptedRequestGeneration() > stamp);

        core.setMox(false);
        drain();
        QVERIFY(!core.mox());
        QCOMPARE(mox->pttMode(), PttMode::None);
    }

    void radioModel_tciUnkey_doesNotReleaseManualKey()
    {
        RadioModel core;
        MoxController* mox = core.moxController();
        QVERIFY(mox != nullptr);
        makeSync(*mox);
        mox->setMoxCheck([]() {
            return safety::BandPlanGuard::MoxCheckResult{true, QString()};
        });

        core.setMoxFromButton(true);
        drain();
        QVERIFY(mox->isManualKey());
        core.setMox(false);
        drain();
        QVERIFY(core.mox());

        core.setMoxFromButton(false);
        drain();
        QVERIFY(!core.mox());
        QVERIFY(!mox->isManualKey());
    }
};

QTEST_GUILESS_MAIN(TestMoxControllerPttSources)
#include "tst_mox_controller_ptt_sources.moc"
