// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_station_tx_gate.cpp  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 34 (R-IOS-02; remote design section 12.2): who may
// transmit. Every refusal first (remote_transmit deny, a hello without
// remoteTx, a device not paired, a snapshot not complete, another device
// holding transmit, a transfer running), then the permitted cases (the
// holder, and every permitted device while transmit is unheld).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), iPhone app plan Task 34 (R-IOS-02), with
//               AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include <QtTest>

#include "core/safety/StationTxGate.h"
#include "core/safety/TransmitHolder.h"

using namespace NereusSDR;

namespace {

SessionPeerInfo ready(const QByteArray& id)
{
    SessionPeerInfo peer;
    peer.deviceId = id;
    peer.declaresRemoteTx = true;
    peer.paired = true;
    peer.snapshotComplete = true;
    return peer;
}

struct Rig {
    TransmitHolder holder;
    StationTxGate gate;
    bool mox{false};
    std::function<void(UnkeyOutcome)> pendingUnkey;

    Rig()
    {
        TransmitHolder::Hooks hooks;
        hooks.clock = []() { return qint64(0); };
        hooks.moxOn = [this]() { return mox; };
        hooks.unkey = [this](const QString&, std::function<void(UnkeyOutcome)> done) {
            pendingUnkey = std::move(done);
        };
        hooks.stopAllTx = [](const QString&) {};
        hooks.disarmVox = []() {};
        hooks.schedule = [](int, std::function<void()>) {};
        hooks.describe = [](const QByteArray& id) -> std::optional<TransmitHolder::Words> {
            if (id == "phone") {
                return TransmitHolder::Words{QStringLiteral("Grant's iPhone"),
                                             QStringLiteral("iPhone"), QStringLiteral("phone")};
            }
            return std::nullopt;
        };
        holder.setHooks(hooks);
        gate.setTransmitHolder(&holder);
        gate.setRemoteTransmitAllowed(true);
    }
};

} // namespace

class TstStationTxGate : public QObject {
    Q_OBJECT

private slots:
    // ---- Refusals -------------------------------------------------------------

    void remoteTransmitDenyRefusesEveryone()
    {
        Rig rig;
        rig.gate.setRemoteTransmitAllowed(false);
        const TxDecision d = rig.gate.decide(ready("phone"));
        QVERIFY(!d.permitted);
        QCOMPARE(d.refusal, TxRefusals::stationReceiveOnly());
    }

    void theDefaultIsDeny()
    {
        StationTxGate gate;
        QVERIFY(!gate.remoteTransmitAllowed());
        QVERIFY(!gate.decide(ready("phone")).permitted);
    }

    void aHelloWithoutRemoteTxIsNeverPermitted()
    {
        Rig rig;
        SessionPeerInfo peer = ready("phone");
        peer.declaresRemoteTx = false;
        const TxDecision d = rig.gate.decide(peer);
        QVERIFY(!d.permitted);
        QCOMPARE(d.refusal, TxRefusals::appCannotTransmit());
    }

    void aDeviceNotPairedIsNotPermitted()
    {
        Rig rig;
        SessionPeerInfo peer = ready("token:1");
        peer.paired = false;
        const TxDecision d = rig.gate.decide(peer);
        QVERIFY(!d.permitted);
        QCOMPARE(d.refusal, TxRefusals::deviceNotPaired());
    }

    void notPermittedUntilTheSnapshotIsComplete()
    {
        Rig rig;
        SessionPeerInfo peer = ready("phone");
        peer.snapshotComplete = false;
        const TxDecision d = rig.gate.decide(peer);
        QVERIFY(!d.permitted);
        QCOMPARE(d.refusal, TxRefusals::notReady());
    }

    void anotherDeviceHoldingTransmitRefusesNamingIt()
    {
        Rig rig;
        QCOMPARE(rig.holder.askKey({"phone", TransmitHolder::Source::Device, false, false}).verdict,
                 KeyingVerdict::Admit);
        const TxDecision d = rig.gate.decide(ready("pad"));
        QVERIFY(!d.permitted);
        QCOMPARE(d.refusal, TxRefusals::otherDeviceHolds(QStringLiteral("Grant's iPhone")));
    }

    void duringATransferNobodyIsPermitted()
    {
        Rig rig;
        QCOMPARE(rig.holder.askKey({"phone", TransmitHolder::Source::Device, false, false}).verdict,
                 KeyingVerdict::Admit);
        rig.mox = true;
        rig.holder.onMoxReading(true);
        rig.holder.setKeyed(true);
        TransmitHolder::Holder next;
        next.deviceId = "pad";
        rig.holder.transferTo(next, QStringLiteral("take"));
        QCOMPARE(rig.gate.decide(ready("phone")).refusal, TxRefusals::changingHands());
        QCOMPARE(rig.gate.decide(ready("pad")).refusal, TxRefusals::changingHands());
    }

    // ---- Permitted ------------------------------------------------------------

    void withTransmitUnheldEveryReadyDeviceIsPermitted()
    {
        Rig rig;
        QVERIFY(rig.gate.decide(ready("phone")).permitted);
        QVERIFY(rig.gate.decide(ready("pad")).permitted);
        QVERIFY(rig.gate.decide(ready("phone")).refusal.isEmpty());
    }

    void theHolderIsPermitted()
    {
        Rig rig;
        QCOMPARE(rig.holder.askKey({"phone", TransmitHolder::Source::Device, false, false}).verdict,
                 KeyingVerdict::Admit);
        QVERIFY(rig.gate.decide(ready("phone")).permitted);
    }

    void withNoHolderObjectTransmitReadsUnheld()
    {
        StationTxGate gate;
        gate.setRemoteTransmitAllowed(true);
        QVERIFY(gate.decide(ready("phone")).permitted);
    }
};

QTEST_MAIN(TstStationTxGate)
#include "tst_station_tx_gate.moc"
