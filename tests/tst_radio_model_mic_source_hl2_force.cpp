// no-port-check: NereusSDR-original unit-test file.
// =================================================================
// tests/tst_radio_model_mic_source_hl2_force.cpp  (NereusSDR)
// =================================================================
//
// Unit tests for Phase 3M-1b Task L.3:
//   HL2 force-Pc on connect — RadioModel + TransmitModel lock guard.
//
// Design source: pre-code review §11.
//   - RadioModel::connectToRadio() calls loadFromSettings(mac) then
//     setMicSourceLocked(!boardCapabilities().hasMicJack).
//   - When locked, TransmitModel::setMicSource(MicSource::Radio)
//     silently coerces to MicSource::Pc.
//   - UI side (AudioTxInputPage) already disables the Radio Mic radio
//     button when !hasMicJack; L.3 completes the model-side lock.
//
// Cases:
//   1. HL2 fresh connect — empty AppSettings → micSource == Pc.
//   2. HL2 connect with stored Radio — forced to Pc regardless.
//   3. Non-HL2 fresh connect — empty AppSettings → default Pc.
//   4. Non-HL2 with stored Radio — loads as Radio (no override).
//   5. Lock prevents setMicSource(Radio) on HL2 → stays Pc.
//   6. Lock released on non-HL2 — setMicSource(Radio) succeeds.
//   7. isMicSourceLocked() reflects hasMicJack.
//   8. Reconnect from HL2 → non-HL2 — lock released and Radio allowed.
//   9. Radio codec lane: the HL2's own capability row (the AK4951 audio
//      add-on board, BoardCapabilities::radioMicNeedsAddOn) is not locked;
//      a stored Radio loads as Radio, as mi0bot has no HL2 mic lock
//      (mi0bot setup.cs:14566-14589 [@c26a8a4]). Cases 1, 2 and 5 inject
//      hasMicJack false with no add-on, a board with neither (the HL2
//      receive-only kit's row), and keep the lock.
//
// NEREUS_BUILD_TESTS is defined in CMakeLists.txt for this target,
// enabling RadioModel test seams (setCapsHasMicJackForTest,
// simulateConnectLoadForTest, simulateDisconnectForTest).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-28 — Original test for NereusSDR by J.J. Boyd (KG4VCF),
//                 with AI-assisted implementation via Anthropic Claude Code.
//   2026-09-30 — Radio codec lane: case 9, the HL2 with its add-on board
//                 is not locked. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include "core/AppSettings.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"
#include "core/audio/CompositeTxMicRouter.h"

using namespace NereusSDR;

static const QString kMacHl2    = QStringLiteral("11:22:33:44:55:66");
static const QString kMacHermes = QStringLiteral("aa:bb:cc:dd:ee:ff");

// Convenience: pre-seed AppSettings with a persisted micSource value.
static void seedMicSource(const QString& mac, MicSource source)
{
    const QString key = QStringLiteral("hardware/%1/tx/Mic_Source").arg(mac);
    AppSettings::instance().setValue(
        key,
        source == MicSource::Radio ? QStringLiteral("Radio") : QStringLiteral("Pc"));
}

class TestRadioModelMicSourceHl2Force : public QObject {
    Q_OBJECT

private slots:

    void initTestCase() { AppSettings::instance().clear(); }
    void init()          { AppSettings::instance().clear(); }
    void cleanup()       { AppSettings::instance().clear(); }

    // =========================================================================
    // §1  HL2 fresh connect — empty AppSettings → micSource forced to Pc
    // =========================================================================
    // Default micSource is Pc even without the lock, but the lock must engage
    // (isMicSourceLocked true) so subsequent setMicSource(Radio) is rejected.
    void hl2FreshConnect_micSourceIsPc()
    {
        RadioModel model;
        model.setCapsHasMicJackForTest(false);   // HL2: no mic jack
        model.simulateConnectLoadForTest(kMacHl2);

        QCOMPARE(model.transmitModel().micSource(), MicSource::Pc);
    }

    void hl2FreshConnect_lockEngaged()
    {
        RadioModel model;
        model.setCapsHasMicJackForTest(false);
        model.simulateConnectLoadForTest(kMacHl2);

        QVERIFY(model.transmitModel().isMicSourceLocked());
    }

    // =========================================================================
    // §2  HL2 connect with stored MicSource::Radio
    //     Even if AppSettings has "Radio" (from a different radio previously
    //     used under the same MAC), the HL2 force overrides it to Pc.
    // =========================================================================
    void hl2ConnectWithStoredRadio_forcedToPc()
    {
        // Pre-seed AppSettings with Radio for the HL2 MAC.
        seedMicSource(kMacHl2, MicSource::Radio);

        RadioModel model;
        model.setCapsHasMicJackForTest(false);   // HL2: no mic jack
        model.simulateConnectLoadForTest(kMacHl2);

        // Must be Pc regardless of what was stored.
        QCOMPARE(model.transmitModel().micSource(), MicSource::Pc);
    }

    void hl2ConnectWithStoredRadio_lockEngaged()
    {
        seedMicSource(kMacHl2, MicSource::Radio);

        RadioModel model;
        model.setCapsHasMicJackForTest(false);
        model.simulateConnectLoadForTest(kMacHl2);

        QVERIFY(model.transmitModel().isMicSourceLocked());
    }

    // =========================================================================
    // §3  Non-HL2 fresh connect — default Pc, no lock
    // =========================================================================
    void nonHl2FreshConnect_defaultsPc()
    {
        RadioModel model;
        model.setCapsHasMicJackForTest(true);    // Hermes / ANAN: has mic jack
        model.simulateConnectLoadForTest(kMacHermes);

        QCOMPARE(model.transmitModel().micSource(), MicSource::Pc);
    }

    void nonHl2FreshConnect_notLocked()
    {
        RadioModel model;
        model.setCapsHasMicJackForTest(true);
        model.simulateConnectLoadForTest(kMacHermes);

        QVERIFY(!model.transmitModel().isMicSourceLocked());
    }

    // =========================================================================
    // §4  Non-HL2 connect with stored MicSource::Radio
    //     No override: stored Radio must survive.
    // =========================================================================
    void nonHl2ConnectWithStoredRadio_loadsRadio()
    {
        seedMicSource(kMacHermes, MicSource::Radio);

        RadioModel model;
        model.setCapsHasMicJackForTest(true);    // hasMicJack → no lock
        model.simulateConnectLoadForTest(kMacHermes);

        QCOMPARE(model.transmitModel().micSource(), MicSource::Radio);
    }

    // =========================================================================
    // §5  Lock prevents setMicSource(Radio) on HL2
    //     After HL2 connect, calling setMicSource(Radio) must be silently
    //     coerced to Pc (lock guard in TransmitModel::setMicSource).
    // =========================================================================
    void hl2LockPreventsSetMicSourceRadio()
    {
        RadioModel model;
        model.setCapsHasMicJackForTest(false);
        model.simulateConnectLoadForTest(kMacHl2);

        // Attempt to set Radio while locked — must be silently coerced.
        model.transmitModel().setMicSource(MicSource::Radio);

        QCOMPARE(model.transmitModel().micSource(), MicSource::Pc);
    }

    // =========================================================================
    // §6  Lock released on non-HL2 — setMicSource(Radio) succeeds
    // =========================================================================
    void nonHl2LockReleased_setMicSourceRadioSucceeds()
    {
        RadioModel model;
        model.setCapsHasMicJackForTest(true);
        model.simulateConnectLoadForTest(kMacHermes);

        // No lock on non-HL2; setMicSource(Radio) must succeed.
        model.transmitModel().setMicSource(MicSource::Radio);

        QCOMPARE(model.transmitModel().micSource(), MicSource::Radio);
    }

    // =========================================================================
    // §7  isMicSourceLocked() reflects hasMicJack
    //     HL2 (hasMicJack=false) → locked; Hermes (hasMicJack=true) → not locked.
    // =========================================================================
    void isMicSourceLocked_reflectsHasMicJack()
    {
        {
            RadioModel model;
            model.setCapsHasMicJackForTest(false);
            model.simulateConnectLoadForTest(kMacHl2);
            QVERIFY(model.transmitModel().isMicSourceLocked());
        }
        {
            RadioModel model;
            model.setCapsHasMicJackForTest(true);
            model.simulateConnectLoadForTest(kMacHermes);
            QVERIFY(!model.transmitModel().isMicSourceLocked());
        }
    }

    // =========================================================================
    // §8  Reconnect from HL2 → non-HL2 — lock released, Radio allowed
    //     simulateDisconnectForTest() releases the lock (mirrors teardownConnection).
    //     After reconnect to non-HL2, setMicSource(Radio) must succeed.
    // =========================================================================
    void reconnectHl2ToNonHl2_lockReleasedAndRadioAllowed()
    {
        RadioModel model;

        // First connect: HL2 — lock engaged.
        model.setCapsHasMicJackForTest(false);
        model.simulateConnectLoadForTest(kMacHl2);
        QVERIFY(model.transmitModel().isMicSourceLocked());
        QCOMPARE(model.transmitModel().micSource(), MicSource::Pc);

        // Disconnect: lock released.
        model.simulateDisconnectForTest();
        QVERIFY(!model.transmitModel().isMicSourceLocked());

        // Reconnect: non-HL2.
        AppSettings::instance().clear();  // fresh settings for the new MAC
        model.setCapsHasMicJackForTest(true);
        model.simulateConnectLoadForTest(kMacHermes);
        QVERIFY(!model.transmitModel().isMicSourceLocked());

        // Must now accept Radio.
        model.transmitModel().setMicSource(MicSource::Radio);
        QCOMPARE(model.transmitModel().micSource(), MicSource::Radio);
    }

    // =========================================================================
    // §9  Radio codec lane: the HL2's own row is not locked
    // =========================================================================
    void realHl2_notLockedAndStoredRadioLoads()
    {
        seedMicSource(kMacHl2, MicSource::Radio);

        RadioModel model;
        model.setHpsdrModelForTest(HPSDRModel::HERMESLITE);
        QVERIFY(!model.boardCapabilities().hasMicJack);
        QVERIFY(model.boardCapabilities().radioMicNeedsAddOn);
        QVERIFY(model.boardCapabilities().radioMicSelectable());
        model.simulateConnectLoadForTest(kMacHl2);

        QVERIFY(!model.transmitModel().isMicSourceLocked());
        QCOMPARE(model.transmitModel().micSource(), MicSource::Radio);
    }
};

QTEST_APPLESS_MAIN(TestRadioModelMicSourceHl2Force)
#include "tst_radio_model_mic_source_hl2_force.moc"
