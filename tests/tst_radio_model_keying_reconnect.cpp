// no-port-check: NereusSDR-original.
// Related to #300: connect-time MoxController wiring must not accumulate
// across recreated TX channels or repeated wiring of the current channel.
// An interlock failsafe armed for an old channel must never key a new one.
// The original intermittent crash in #300 has no proven root cause here.
//
// Modification history (NereusSDR):
//   2026-10-01 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex:
//              regression coverage for keying connection/session lifetime.

#include <QtTest/QtTest>
#include <QSignalSpy>

#include <memory>

#include "core/AppSettings.h"
#include "core/MoxController.h"
#include "core/PureSignal.h"
#include "core/TxChannel.h"
#include "core/WdspEngine.h"
#include "models/RadioModel.h"

namespace NereusSDR {

class TstRadioModelKeyingReconnect : public QObject {
    Q_OBJECT

private:
    // Match the real PureSignal-before-TxChannel destruction order, without
    // starting WDSP, a microphone, a radio transport or a worker thread.
    static void retireSession(RadioModel& model)
    {
        model.setTxaFlushedPureSignalObserverForTest({});
        model.m_pureSignal.reset();
        model.injectTxChannelForTest(nullptr);
    }

    struct Rig {
        // RadioModel is destroyed before the final channel, so its teardown
        // never sees a dangling channel through PureSignal or an injected view.
        std::unique_ptr<TxChannel> channel;
        RadioModel model;
        int rfOpens{0};
        int pureSignalUnkeys{0};

        ~Rig()
        {
            retireSession(model);
            if (channel) {
                channel->setRfGateObserverForTest({});
            }
        }

        void beginSession()
        {
            retireSession(model);
            channel.reset();
            rfOpens = 0;
            pureSignalUnkeys = 0;
            channel = std::make_unique<TxChannel>(WdspEngine::kTxChannelId);
            channel->setRfGateObserverForTest([this](bool open) {
                if (open) {
                    ++rfOpens;
                }
            });
            model.injectTxChannelForTest(channel.get());
            model.installPureSignalForTest(channel.get());
            model.setTxaFlushedPureSignalObserverForTest([this]() {
                ++pureSignalUnkeys;
            });
            model.wireTxChannelKeyingForTest();
        }
    };

    static void checkOneKeyAndUnkey(Rig& rig)
    {
        QSignalSpy drained(rig.channel.get(), &TxChannel::txDrained);
        emit rig.model.moxController()->txReady();
        QVERIFY(rig.channel->isRfGateOpen());
        QCOMPARE(rig.rfOpens, 1);

        emit rig.model.moxController()->txDrainRequested();
        QVERIFY(!rig.channel->isRfGateOpen());
        QCOMPARE(rig.pureSignalUnkeys, 1);
        QCOMPARE(drained.count(), 1);
        // Every start/drain request has its own production sequence. One
        // real start followed by one real drain yields 2; duplicate starts
        // are visible here even though opening an already-open gate emits
        // no second gate-change notification.
        QCOMPARE(drained.at(0).at(0).toULongLong(), quint64{2});
    }

private slots:
    void init() { AppSettings::instance().clear(); }
    void cleanup() { AppSettings::instance().clear(); }

    void threeChannelLifetimesDeliverOneStartAndPureSignalUnkeyEach()
    {
        Rig rig;
        for (int session = 0; session < 3; ++session) {
            rig.beginSession();
            checkOneKeyAndUnkey(rig);
        }
    }

    void repeatedCurrentChannelWiringDeliversOneStartAndUnkey()
    {
        Rig rig;
        rig.beginSession();
        rig.model.wireTxChannelKeyingForTest();
        rig.model.wireTxChannelKeyingForTest();
        checkOneKeyAndUnkey(rig);
    }

    void retiredSessionFailsafeCannotStartReplacementChannel()
    {
        Rig rig;
        rig.beginSession();
        rig.model.m_awaitingInterlockForTx = true;
        emit rig.model.moxController()->txReady();
        QVERIFY(!rig.channel->isRfGateOpen());

        rig.beginSession();
        // The replacement is independently awaiting its interlock. It has
        // received no txReady: the old timeout has no authority to start it.
        rig.model.m_awaitingInterlockForTx = true;

        // Arm a later current-session timeout on the same event loop. Its
        // observable completion proves the earlier old-session deadline has
        // passed, without a fixed sleep or an artificially short production
        // timeout. Suppressing every failsafe also fails this positive control.
        Rig control;
        control.beginSession();
        control.model.m_awaitingInterlockForTx = true;
        emit control.model.moxController()->txReady();
        QTRY_VERIFY_WITH_TIMEOUT(control.channel->isRfGateOpen(), 5000);
        QCOMPARE(control.rfOpens, 1);
        QVERIFY(!control.model.m_awaitingInterlockForTx);

        QVERIFY(!rig.channel->isRfGateOpen());
        QCOMPARE(rig.rfOpens, 0);
        QVERIFY(rig.model.m_awaitingInterlockForTx);
    }

    void currentSessionInterlockFailsafeStillStartsItsChannel()
    {
        Rig rig;
        rig.beginSession();
        rig.model.m_awaitingInterlockForTx = true;
        emit rig.model.moxController()->txReady();
        QVERIFY(!rig.channel->isRfGateOpen());

        // A positive control: preserving the existing failsafe is part of
        // the fix; suppressing every timeout would hide the stale-key bug.
        // Repeating wiring on this live channel must preserve that timeout.
        rig.model.wireTxChannelKeyingForTest();
        QTRY_VERIFY_WITH_TIMEOUT(rig.channel->isRfGateOpen(), 5000);
        QCOMPARE(rig.rfOpens, 1);
        QVERIFY(!rig.model.m_awaitingInterlockForTx);
    }
};

} // namespace NereusSDR

QTEST_GUILESS_MAIN(NereusSDR::TstRadioModelKeyingReconnect)
#include "tst_radio_model_keying_reconnect.moc"
