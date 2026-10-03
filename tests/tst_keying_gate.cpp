// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_keying_gate.cpp  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 34 (R-IOS-02; the several-devices design, rulings
// 8.5, 8.8, 8.9 and 8.13): MoxController's keying gate.
//
//  - A refused press leaves the PTT mode as it was. A gate placed inside
//    setMox, after the mode is set, fails refusedPressLeavesThePttMode:
//    the mode would change and change back (two pttModeChanged).
//  - The radio's PTT level, repeated on every status frame, asks the gate
//    once per edge.
//  - A release by keyer X never unkeys keyer Y's key; the station's own
//    releases (its mic, its MOX button) release only its own key; the
//    Core's safety stop (setMox(false)) unkeys whoever is keyed; unkeying
//    never asks the gate.
//  - A take then keys only if the press is still down.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), iPhone app plan Task 34 (R-IOS-02), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-26: iPhone app plan Task 77 (R-IOS-02, R-IOS-03, R-IOS-13): the
//               mic press while another device's key is on asks the gate
//               once. J.J. Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-09-26: Task 77 fix wave, I1: the press asks while another device
//               holds, whatever its key. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
// =================================================================

#include <QtTest>
#include <QScopeGuard>

#include "core/MoxController.h"

using namespace NereusSDR;

namespace {

KeyerIdentity remote(const QByteArray& id)
{
    KeyerIdentity keyer;
    keyer.deviceId = id;
    keyer.source = PttMode::None;
    return keyer;
}

// A gate whose answer the test sets, counting what it is asked.
struct Gate {
    KeyingAnswer answer;
    int asked{0};
    QList<QPair<PttMode, KeyerIdentity>> requests;

    MoxController::KeyingGateFn fn()
    {
        return [this](PttMode source, const KeyerIdentity& keyer) {
            ++asked;
            requests.append({source, keyer});
            return answer;
        };
    }
    void admit() { answer = KeyingAnswer{KeyingVerdict::Admit, {}}; }
    void refuse(const QString& holder)
    {
        answer = KeyingAnswer{KeyingVerdict::Refuse, TxRefusals::otherDeviceHolds(holder)};
    }
};

struct Rig {
    MoxController mox;
    Gate gate;
    Rig()
    {
        mox.setTimerIntervals(0, 0, 0, 0, 0, 0);
        gate.admit();
        mox.setKeyingGate(gate.fn());
    }
    void settle() { QTRY_VERIFY(mox.state() == MoxState::Rx || mox.state() == MoxState::Tx); }
};

} // namespace

class TstKeyingGate : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() { qRegisterMetaType<NereusSDR::TxRefusal>(); }

    // ---- Refusals first -------------------------------------------------------

    void refusedPressLeavesThePttMode()
    {
        Rig rig;
        rig.gate.refuse(QStringLiteral("iPhone"));
        QSignalSpy modes(&rig.mox, &MoxController::pttModeChanged);
        QSignalSpy refused(&rig.mox, &MoxController::moxRefused);
        rig.mox.onMicPttFromRadio(true);
        QVERIFY(!rig.mox.isMox());
        QCOMPARE(rig.mox.pttMode(), PttMode::None);
        QCOMPARE(modes.count(), 0);
        QCOMPARE(refused.count(), 1);
        QCOMPARE(refused.at(0).at(0).value<TxRefusal>(),
                 TxRefusals::otherDeviceHolds(QStringLiteral("iPhone")));
        // Asked with the radio's PTT as the source, for the station device.
        QCOMPARE(rig.gate.requests.first().first, PttMode::Mic);
        QVERIFY(rig.gate.requests.first().second.isStation());
    }

    void theRadiosPttLevelAsksOncePerEdge()
    {
        Rig rig;
        rig.gate.refuse(QStringLiteral("iPhone"));
        QSignalSpy refused(&rig.mox, &MoxController::moxRefused);
        for (int frame = 0; frame < 10; ++frame) {
            rig.mox.onMicPttFromRadio(true);   // every status frame
        }
        QCOMPARE(rig.gate.asked, 1);
        QCOMPARE(refused.count(), 1);
        for (int frame = 0; frame < 10; ++frame) {
            rig.mox.onMicPttFromRadio(false);
        }
        QCOMPARE(rig.gate.asked, 1);
        // The next press is a new edge.
        rig.mox.onMicPttFromRadio(true);
        rig.mox.onMicPttFromRadio(true);
        QCOMPARE(rig.gate.asked, 2);
        QCOMPARE(refused.count(), 2);
    }

    void aRefusedAppLevelIsDropped()
    {
        // A program's refused key (CAT, TCI) is answered as not keyed.
        Rig rig;
        rig.gate.answer = KeyingAnswer{KeyingVerdict::Refuse, TxRefusals::programNeedsTransmit()};
        rig.mox.onTciPtt(true);
        QVERIFY(!rig.mox.isMox());
        QVERIFY(!rig.mox.isTciPttHeld());
        QVERIFY(rig.gate.requests.first().second.program);
        QCOMPARE(rig.gate.requests.first().first, PttMode::Tci);
    }

    void aStationKeyWhileAnotherDevicesKeyIsOnIsRefused()
    {
        Rig rig;
        rig.mox.setMox(true, remote("phone"));
        rig.settle();
        QVERIFY(rig.mox.isMox());
        rig.gate.refuse(QStringLiteral("iPhone"));
        QSignalSpy refused(&rig.mox, &MoxController::moxRefused);
        rig.mox.setMox(true);       // the MOX button's key
        rig.mox.onMoxButton(true);
        QCOMPARE(refused.count(), 2);
        QCOMPARE(rig.mox.currentKeyer().deviceId, QByteArray("phone"));
        QVERIFY(rig.mox.admitStationKey(PttMode::Manual) == false);
    }

    void aRemoteKeyTheGateRefusesChangesNothing()
    {
        Rig rig;
        rig.gate.refuse(QStringLiteral("Radio"));
        QSignalSpy refused(&rig.mox, &MoxController::moxRefused);
        rig.mox.setMox(true, remote("phone"));
        QVERIFY(!rig.mox.isMox());
        QCOMPARE(refused.count(), 1);
        QCOMPARE(rig.gate.requests.first().second.deviceId, QByteArray("phone"));
    }

    // ---- Releases by keyer (ruling 8.5) ---------------------------------------

    void aReleaseByKeyerXNeverUnkeysKeyerY()
    {
        Rig rig;
        rig.mox.setMox(true, remote("phone"));
        rig.settle();
        QVERIFY(rig.mox.isMox());
        QCOMPARE(rig.mox.currentKeyer().deviceId, QByteArray("phone"));
        const int askedBefore = rig.gate.asked;

        rig.mox.setMox(false, remote("pad"));
        QVERIFY(rig.mox.isMox());
        // The station's own releases: its mic released, its MOX button off.
        rig.mox.onMicPttFromRadio(true);
        rig.mox.onMicPttFromRadio(false);
        rig.mox.onMoxButton(false);
        QVERIFY(rig.mox.isMox());
        QCOMPARE(rig.mox.currentKeyer().deviceId, QByteArray("phone"));

        // Its own release unkeys it, and unkeying never asks the gate.
        rig.mox.setMox(false, remote("phone"));
        QVERIFY(!rig.mox.isMox());
        QTRY_COMPARE(rig.mox.state(), MoxState::Rx);
        // Task 77 (ruling 8.9): the station's mic press while another
        // device's key was on asked the gate once, at its press edge (a
        // take), and keyed nothing then; the releases asked nothing.
        QCOMPARE(rig.gate.asked, askedBefore + 1);
        QCOMPARE(rig.gate.requests.last().first, PttMode::Mic);
        QVERIFY(rig.gate.requests.last().second.isStation());
    }

    // Task 77 fix wave, I1 (ruling 8.9): while another device holds
    // transmit, the radio's press asks the gate whatever that device's key:
    // a manual key (TUNE, two-tone, a tuner autotune set it before keying)
    // and a VOX key (the station's VOX source, the holder's key) alike. A
    // press that takes nothing is held off until it is released.
    void aPressAsksTheGateWhileAnotherDeviceHoldsWhateverItsKey()
    {
        // A device's two-tone or TUNE: the manual key, keyed as the device.
        {
            Rig rig;
            rig.mox.setOtherDeviceHolds([]() { return true; });
            rig.mox.setManualKey(true);
            rig.mox.setMox(true, remote("phone"));
            rig.settle();
            QVERIFY(rig.mox.isMox());
            const int before = rig.gate.asked;
            rig.gate.answer = KeyingAnswer{KeyingVerdict::Take, {}};
            rig.mox.onMicPttFromRadio(true);
            QCOMPARE(rig.gate.asked, before + 1);
            QCOMPARE(rig.gate.requests.last().first, PttMode::Mic);
            QVERIFY(rig.gate.requests.last().second.isStation());
            // Every status frame repeats the level: asked once per edge.
            rig.mox.onMicPttFromRadio(true);
            QCOMPARE(rig.gate.asked, before + 1);
            QCOMPARE(rig.mox.currentKeyer().deviceId, QByteArray("phone"));
            rig.mox.onMicPttFromRadio(false);
            rig.mox.setMox(false, remote("phone"));
            rig.mox.setManualKey(false);
            rig.settle();
        }
        // The device's VOX key: the station's VOX source, the holder's key.
        {
            Rig rig;
            rig.mox.setOtherDeviceHolds([]() { return true; });
            rig.mox.onVoxActive(true);
            rig.settle();
            QVERIFY(rig.mox.isMox());
            QVERIFY(rig.mox.currentKeyer().isStation());
            const int before = rig.gate.asked;
            rig.gate.answer = KeyingAnswer{KeyingVerdict::Take, {}};
            rig.mox.onMicPttFromRadio(true);
            QCOMPARE(rig.gate.asked, before + 1);
            QCOMPARE(rig.gate.requests.last().first, PttMode::Mic);
            // The press keyed nothing and did not rename the VOX key.
            QCOMPARE(rig.mox.pttMode(), PttMode::Vox);
            rig.mox.onMicPttFromRadio(false);
            rig.mox.onVoxActive(false);
            rig.settle();
        }
        // A press the gate refuses (transmit changing hands) is held off:
        // once the holder's key ends, the still-held PTT keys nothing
        // without a fresh press.
        {
            Rig rig;
            bool otherHolds = true;
            rig.mox.setOtherDeviceHolds([&otherHolds]() { return otherHolds; });
            rig.mox.setManualKey(true);
            rig.mox.setMox(true, remote("phone"));
            rig.settle();
            rig.gate.answer = KeyingAnswer{KeyingVerdict::Refuse, TxRefusals::changingHands()};
            rig.mox.onMicPttFromRadio(true);
            rig.mox.setMox(false, remote("phone"));
            rig.mox.setManualKey(false);
            QTRY_COMPARE(rig.mox.state(), MoxState::Rx);
            otherHolds = false;
            rig.gate.admit();
            for (int frame = 0; frame < 5; ++frame) {
                rig.mox.onMicPttFromRadio(true);
            }
            QVERIFY(!rig.mox.isMox());
            // The next press keys.
            rig.mox.onMicPttFromRadio(false);
            rig.mox.onMicPttFromRadio(true);
            QVERIFY(rig.mox.isMox());
            rig.mox.onMicPttFromRadio(false);
            rig.settle();
        }
    }

    // Fix wave M1 still holds for a holder that is not on the air: a press
    // the band plan refuses takes nothing (the gate is never asked). A
    // holder on the air is taken from all the same (ruling 8.9).
    void aPressRefusedBeforeTheGateTakesFromAnUnkeyedHolderNothing()
    {
        Rig rig;
        rig.mox.setOtherDeviceHolds([]() { return true; });
        rig.mox.setMoxCheck([]() {
            safety::BandPlanGuard::MoxCheckResult r;
            r.ok = false;
            r.reason = QStringLiteral("Frequency outside TX-allowed range");
            return r;
        });
        rig.mox.onMicPttFromRadio(true);
        QCOMPARE(rig.gate.asked, 0);
        QVERIFY(!rig.mox.isMox());
        rig.mox.onMicPttFromRadio(false);

        // On the air (the device's own key passed the check before it came
        // on): the press asks the gate.
        rig.mox.setMoxCheck({});
        rig.mox.setMox(true, remote("phone"));
        rig.settle();
        rig.mox.setMoxCheck([]() {
            safety::BandPlanGuard::MoxCheckResult r;
            r.ok = false;
            r.reason = QStringLiteral("Frequency outside TX-allowed range");
            return r;
        });
        const int before = rig.gate.asked;
        rig.gate.answer = KeyingAnswer{KeyingVerdict::Take, {}};
        rig.mox.onMicPttFromRadio(true);
        QCOMPARE(rig.gate.asked, before + 1);
        rig.mox.onMicPttFromRadio(false);
        rig.mox.setMox(false, remote("phone"));
        rig.settle();
    }

    // Without the holder's state (no Core serving devices) the press edge
    // asks only while another keyer's key is on, as before.
    void withoutTheHoldersStateAManualKeyIsNotAsked()
    {
        Rig rig;
        rig.mox.setManualKey(true);
        rig.mox.setMox(true, remote("phone"));
        rig.settle();
        const int before = rig.gate.asked;
        rig.mox.onMicPttFromRadio(true);
        QCOMPARE(rig.gate.asked, before);
        rig.mox.onMicPttFromRadio(false);
        rig.mox.setMox(false, remote("phone"));
        rig.mox.setManualKey(false);
        rig.settle();
    }

    void aStationMicPressDoesNotRenameAnotherDevicesKey()
    {
        Rig rig;
        rig.mox.setMox(true, remote("phone"));
        rig.settle();
        rig.mox.onMicPttFromRadio(true);
        QCOMPARE(rig.mox.pttMode(), PttMode::None);
        rig.mox.onMicPttFromRadio(false);
        QVERIFY(rig.mox.isMox());
    }

    void theSafetyStopUnkeysWhoeverIsKeyed()
    {
        Rig rig;
        rig.mox.setMox(true, remote("phone"));
        rig.settle();
        rig.mox.setMox(false);   // the Core's safety stops (watchdog, time-out, ...)
        QVERIFY(!rig.mox.isMox());
        QVERIFY(rig.mox.currentKeyer().isStation());
    }

    // ---- The take arm (ruling 8.9) --------------------------------------------

    void aTakeKeysOnlyIfThePressIsStillDown()
    {
        Rig rig;
        rig.gate.answer = KeyingAnswer{KeyingVerdict::Take, {}};
        QSignalSpy modes(&rig.mox, &MoxController::pttModeChanged);
        rig.mox.onMicPttFromRadio(true);
        QVERIFY(!rig.mox.isMox());
        QCOMPARE(modes.count(), 0);
        // Repeated frames do not ask again while the take runs.
        rig.mox.onMicPttFromRadio(true);
        QCOMPARE(rig.gate.asked, 1);

        // The take ended; the press is still down: it keys now, through the
        // gate, for the station device.
        rig.gate.admit();
        rig.mox.onTakeFinished(KeyerIdentity::station(PttMode::Mic), true);
        QVERIFY(rig.mox.isMox());
        QCOMPARE(rig.mox.pttMode(), PttMode::Mic);
        QCOMPARE(rig.gate.asked, 2);
    }

    void aPressReleasedDuringTheTakeKeysNothing()
    {
        Rig rig;
        rig.gate.answer = KeyingAnswer{KeyingVerdict::Take, {}};
        rig.mox.onMicPttFromRadio(true);
        rig.mox.onMicPttFromRadio(false);
        rig.gate.admit();
        rig.mox.onTakeFinished(KeyerIdentity::station(PttMode::Mic), true);
        QVERIFY(!rig.mox.isMox());
    }

    void aFailedTakeKeysNothing()
    {
        Rig rig;
        rig.gate.answer = KeyingAnswer{KeyingVerdict::Take, {}};
        rig.mox.onMicPttFromRadio(true);
        rig.gate.admit();
        rig.mox.onTakeFinished(KeyerIdentity::station(PttMode::Mic), false);
        QVERIFY(!rig.mox.isMox());
    }

    // ---- Admit paths ----------------------------------------------------------

    void stationPollFlavorsAskTheGateOnce_data()
    {
        QTest::addColumn<int>("mode");
        QTest::newRow("mic") << int(PttMode::Mic);
        QTest::newRow("cat") << int(PttMode::Cat);
        QTest::newRow("vox") << int(PttMode::Vox);
        QTest::newRow("tci") << int(PttMode::Tci);
    }

    void stationPollFlavorsAskTheGateOnce()
    {
        QFETCH(int, mode);
        Rig rig;
        const auto poll = [&](bool down) {
            switch (PttMode(mode)) {
            case PttMode::Mic: rig.mox.onMicPttFromRadio(down); break;
            case PttMode::Cat: rig.mox.onCatPtt(down); break;
            case PttMode::Vox: rig.mox.onVoxActive(down); break;
            case PttMode::Tci: rig.mox.onTciPtt(down); break;
            default: QFAIL("unexpected poll flavor");
            }
        };
        poll(true);
        QVERIFY(rig.mox.isMox());
        QCOMPARE(rig.gate.asked, 1);
        QVERIFY(rig.mox.currentKeyer().isStation());
        QCOMPARE(rig.mox.currentKeyer().source, PttMode(mode));
        poll(false);
        QVERIFY(!rig.mox.isMox());
        rig.settle();
        poll(true);
        QCOMPARE(rig.gate.asked, 2); // new edge gets its own single admission
        poll(false);
        rig.settle();
    }

    void reentrantStationPollCannotBorrowOrEraseRemoteAdmission()
    {
        Rig rig;
        KeyerIdentity phone = remote("phone");
        phone.session = QStringLiteral("station:radio-owner");
        int remoteChecks = 0;
        bool stationChecked = false;
        rig.mox.setMoxCheck([&]() {
            const auto attempt = rig.mox.keyAttemptIdentity();
            safety::BandPlanGuard::MoxCheckResult result;
            if (attempt && attempt->isStation()) {
                stationChecked = true;
                result.ok = false; // local capture is unavailable; remote admission cannot exempt it
                result.reason = QStringLiteral("Local microphone not ready");
            } else {
                if (++remoteChecks == 2) { rig.mox.onCatPtt(true); }
                result.ok = true;
            }
            return result;
        });
        const auto cleanup = qScopeGuard([&]() { rig.mox.setMoxCheck({}); rig.mox.setMox(false); });
        rig.mox.setMox(true, phone);
        QVERIFY(stationChecked);
        QVERIFY(rig.mox.isMox());
        QCOMPARE(rig.mox.currentKeyer().deviceId, phone.deviceId);
        QCOMPARE(rig.mox.currentKeyer().session, phone.session);
        QCOMPARE(rig.gate.asked, 1);
        QVERIFY(!rig.mox.keyAttemptIdentity());
        rig.mox.setMoxCheck({});
        rig.mox.setMox(false, phone);
        rig.settle();
        rig.mox.onCatPtt(false);
    }

    void anAdmittedPressKeysForTheStationDevice()
    {
        Rig rig;
        rig.mox.onMicPttFromRadio(true);
        QVERIFY(rig.mox.isMox());
        QCOMPARE(rig.mox.pttMode(), PttMode::Mic);
        QVERIFY(rig.mox.currentKeyer().isStation());
        QCOMPARE(rig.mox.currentKeyer().source, PttMode::Mic);
        QCOMPARE(rig.gate.asked, 1);   // once: not again inside setMox
        rig.mox.onMicPttFromRadio(false);
        QVERIFY(!rig.mox.isMox());
    }

    void aRemoteKeyIsAskedWithItsKeyerAndIsItsOwn()
    {
        Rig rig;
        KeyerIdentity phone = remote("phone");
        rig.mox.setMox(true, phone);
        QVERIFY(rig.mox.isMox());
        QCOMPARE(rig.gate.asked, 1);
        QCOMPARE(rig.gate.requests.first().second, phone);
        QCOMPARE(rig.mox.currentKeyer(), phone);
    }

    void withNoGateEveryKeyIsAdmittedAsBefore()
    {
        MoxController mox;
        mox.setTimerIntervals(0, 0, 0, 0, 0, 0);
        QVERIFY(!mox.hasKeyingGate());
        mox.onMicPttFromRadio(true);
        QVERIFY(mox.isMox());
        QVERIFY(mox.admitStationKey(PttMode::Manual));
        mox.onMicPttFromRadio(false);
        QVERIFY(!mox.isMox());
    }

    void theStationKeyerNamesItsSourceAndProgram()
    {
        QVERIFY(KeyerIdentity::station(PttMode::Tci).program);
        QVERIFY(KeyerIdentity::station(PttMode::Cat).program);
        QVERIFY(!KeyerIdentity::station(PttMode::Mic).program);
        QVERIFY(!KeyerIdentity::station(PttMode::Vox).program);
        QCOMPARE(KeyerIdentity::station(PttMode::Mic).deviceId, QByteArray("station"));
    }

    // ---- Fix wave M1: checks before the gate ------------------------------

    // TX inhibit, the PA trip, receive only, the band plan and the
    // interlock refuse a key before the gate is asked, so it takes nothing:
    // a remote key, a station button and a radio PTT press alike.
    void aKeyRefusedBeforeTheGateNeverAsksIt()
    {
        Rig rig;
        QSignalSpy refused(&rig.mox, &MoxController::moxRefused);
        rig.mox.setTxInhibited(true);
        rig.mox.setMox(true, remote("phone"));
        rig.mox.setMox(true);
        rig.mox.onMicPttFromRadio(true);
        rig.mox.onMicPttFromRadio(false);
        QCOMPARE(rig.gate.asked, 0);
        QVERIFY(!rig.mox.isMox());
        QVERIFY(refused.count() >= 1);
        QCOMPARE(refused.first().first().value<TxRefusal>(), TxRefusals::txInhibited());
        rig.mox.setTxInhibited(false);

        // The band plan (the MOX check).
        rig.mox.setMoxCheck([]() {
            safety::BandPlanGuard::MoxCheckResult r;
            r.ok = false;
            r.reason = QStringLiteral("Frequency outside TX-allowed range");
            return r;
        });
        rig.mox.setMox(true, remote("phone"));
        rig.mox.setMox(true);
        QCOMPARE(rig.gate.asked, 0);
        QVERIFY(!rig.mox.isMox());
        QCOMPARE(rig.mox.lastRefusal().code, QByteArray(TxRefusals::kBandPlan));

        // With the checks passing the gate is asked, and the key keys.
        rig.mox.setMoxCheck({});
        rig.mox.setMox(true, remote("phone"));
        QCOMPARE(rig.gate.asked, 1);
        QVERIFY(rig.mox.isMox());
        rig.mox.setMox(false, remote("phone"));
        rig.settle();
    }
};

QTEST_MAIN(TstKeyingGate)
#include "tst_keying_gate.moc"
