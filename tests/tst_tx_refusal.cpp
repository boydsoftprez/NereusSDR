// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_tx_refusal.cpp  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 34 (R-IOS-13): every transmit refusal has its code,
// its plain sentence and its fix, as the link document's Transmit section
// lists them; names such as "Grant's iPhone" put into a sentence keep it
// plain; and each refusal path of MoxController (TX inhibit, the PA trip,
// the band plan and the microphone check, the interlock with the amplifier
// in standby and with the SWR over its limit, the keying gate) produces its
// TxRefusal through moxRefused.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), iPhone app plan Task 34 (R-IOS-13), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-26: Transmit group fix wave C1: micNotConnected. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
// =================================================================

#include <QtTest>

#include "core/MoxController.h"
#include "core/TxInterlockPolicy.h"
#include "core/safety/TxRefusal.h"

#include "OperatorWording.h"

using namespace NereusSDR;

namespace {

struct Row {
    TxRefusal refusal;
    QByteArray code;
    QString text;
    QByteArray fix;
};

// The link document's Transmit section, row for row.
QList<Row> table()
{
    using namespace TxRefusals;
    return {
        {notReady(), "notReady",
         QStringLiteral("This device is still connecting to the Core. Try again in a moment."), {}},
        {appCannotTransmit(), "notReady",
         QStringLiteral("Update this app to transmit through this Core."), {}},
        {deviceNotPaired(), "notReady",
         QStringLiteral("Pair this device with the Core to transmit through it."), {}},
        {stationReceiveOnly(), "stationReceiveOnly",
         QStringLiteral("This Core is set to receive only."), {}},
        {bandPlan(QStringLiteral("Frequency outside TX-allowed range")), "bandPlan",
         QStringLiteral("Frequency outside TX-allowed range"), {}},
        {txInhibited(), "interlock",
         QStringLiteral("The radio's transmit inhibit input is holding transmit off."), {}},
        {interlock(), "interlock",
         QStringLiteral("The transmit interlock is holding transmit off. Check it in Setup."), {}},
        {ampStandby(), "ampStandby",
         QStringLiteral("The amplifier is in standby. Operate it, or change the interlock in Setup."),
         "operateAmp"},
        {paProtection(), "paProtection",
         QStringLiteral("The amplifier has tripped. Reset it before transmitting."), {}},
        {swr(), "swr",
         QStringLiteral("The SWR is over the interlock's limit. Check the antenna, or change the "
                        "interlock in Setup."),
         {}},
        {otherDeviceHolds(QStringLiteral("iPhone")), "otherDeviceHolds",
         QStringLiteral("iPhone has the transmitter."), "takeTransmit"},
        {otherDeviceHolds(QStringLiteral("Radio")), "otherDeviceHolds",
         QStringLiteral("Radio has the transmitter."), "takeTransmit"},
        {programNeedsTransmit(), "programNeedsTransmit",
         QStringLiteral("A program can transmit only while this device has transmit. "
                        "Take transmit here first."),
         "takeTransmit"},
        {micNotReady(), "micNotReady",
         QStringLiteral("Microphone is not ready. Check Audio settings and retry."), {}},
        // Fix wave M4: a remote line that carried no sound in time.
        {remoteMicNotReady(), "micNotReady",
         QStringLiteral("No sound has reached the Core from this device's microphone. "
                        "Wait a moment and try again."),
         {}},
        // Fix wave C1: a remote voice key with no microphone line.
        {micNotConnected(), "micNotReady",
         QStringLiteral("This device's microphone is not connected to the Core. "
                        "Wait a moment and try again."),
         {}},
        {changingHands(), "changingHands",
         QStringLiteral("Transmit is changing hands. Try again in a moment."), {}},
        {stopNotConfirmed(), "stopNotConfirmed",
         QStringLiteral("The radio did not confirm it stopped transmitting."), {}},
        {holderOnAir(QStringLiteral("iPhone"), false), "holderOnAir",
         QStringLiteral("iPhone is on the air. Try again when they stop."), "takeTransmit"},
        {holderOnAir(QStringLiteral("Radio"), true), "holderOnAir",
         QStringLiteral("The radio is on the air. Try again when it stops."), "takeTransmit"},
        {notHolder(), "notHolder", QStringLiteral("Take transmit on this device first."),
         "takeTransmit"},
    };
}

} // namespace

class TstTxRefusal : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() { qRegisterMetaType<NereusSDR::TxRefusal>(); }

    void everyRefusalHasItsCodeTextAndFix()
    {
        for (const Row& row : table()) {
            QCOMPARE(row.refusal.code, row.code);
            QCOMPARE(row.refusal.text, row.text);
            QCOMPARE(row.refusal.fix, row.fix);
            QVERIFY2(OperatorWording::isPlain(row.refusal.text),
                     qPrintable(row.refusal.text));
            QVERIFY2(OperatorWording::coreCalledStationIn(row.refusal.text).isEmpty(),
                     qPrintable(row.refusal.text));
        }
    }

    // A device's name is the operator's own word (ruling 4.3) and is never
    // held to the wording rules: "Grant" itself names an internal term
    // ("grant") on OperatorWording's list. The sentence around the name is
    // held: with the name in place it reads exactly as sent, and with the
    // name taken out it is plain.
    void namesPutIntoASentenceKeepItPlain()
    {
        const QStringList names{QStringLiteral("Grant's iPhone"), QStringLiteral("iPad 2"),
                                QStringLiteral("Shack computer"), QStringLiteral("Radio")};
        for (const QString& name : names) {
            const TxRefusal holds = TxRefusals::otherDeviceHolds(name);
            QCOMPARE(holds.text, QStringLiteral("%1 has the transmitter.").arg(name));
            QVERIFY2(OperatorWording::isPlain(QString(holds.text).replace(name, QStringLiteral("iPhone"))),
                     qPrintable(holds.text));
            const TxRefusal onAir = TxRefusals::holderOnAir(name, false);
            QCOMPARE(onAir.text, QStringLiteral("%1 is on the air. Try again when they stop.").arg(name));
            QVERIFY2(OperatorWording::isPlain(QString(onAir.text).replace(name, QStringLiteral("iPhone"))),
                     qPrintable(onAir.text));
        }
    }

    void anUnplainBandPlanReasonIsNotPassedOn()
    {
        // A band-plan reason is the guard's own sentence; one that names an
        // internal term is replaced by the band plan's general sentence.
        const TxRefusal refusal = TxRefusals::bandPlan(QStringLiteral("DSP slot out of range"));
        QCOMPARE(refusal.code, QByteArray("bandPlan"));
        QVERIFY(OperatorWording::isPlain(refusal.text));
        QCOMPARE(refusal.text, QStringLiteral("The band plan does not allow transmitting here."));
    }

    // ---- MoxController's refusal paths ------------------------------------

    void txInhibitRefusesWithInterlock()
    {
        MoxController mox;
        QSignalSpy refused(&mox, &MoxController::moxRefused);
        mox.setTxInhibited(true);
        mox.setMox(true);
        QVERIFY(!mox.isMox());
        QCOMPARE(refused.count(), 1);
        QCOMPARE(refused.at(0).at(0).value<TxRefusal>(), TxRefusals::txInhibited());
    }

    void paTripRefusesWithPaProtection()
    {
        MoxController mox;
        QSignalSpy refused(&mox, &MoxController::moxRefused);
        mox.setPaTripped(true);
        mox.setMox(true);
        QCOMPARE(refused.count(), 1);
        QCOMPARE(refused.at(0).at(0).value<TxRefusal>(), TxRefusals::paProtection());
    }

    void theBandPlanCheckRefusesWithItsCode()
    {
        MoxController mox;
        QSignalSpy refused(&mox, &MoxController::moxRefused);
        mox.setMoxCheck([]() {
            safety::BandPlanGuard::MoxCheckResult r{false,
                QStringLiteral("Frequency outside TX-allowed range")};
            return r;
        });
        mox.setMox(true);
        QCOMPARE(refused.count(), 1);
        QCOMPARE(refused.at(0).at(0).value<TxRefusal>(),
                 TxRefusals::bandPlan(QStringLiteral("Frequency outside TX-allowed range")));
    }

    void theCheckNamesItsOwnCode()
    {
        // The microphone check and the receive-only Core ride the same
        // callback and name their codes.
        MoxController mox;
        QSignalSpy refused(&mox, &MoxController::moxRefused);
        mox.setMoxCheck([]() {
            safety::BandPlanGuard::MoxCheckResult r{false, QStringLiteral("unused"), true};
            r.refusalCode = TxRefusals::kMicNotReady;
            return r;
        });
        mox.setMox(true);
        QCOMPARE(refused.count(), 1);
        QCOMPARE(refused.at(0).at(0).value<TxRefusal>(), TxRefusals::micNotReady());

        mox.setMoxCheck([]() {
            safety::BandPlanGuard::MoxCheckResult r{false, QStringLiteral("unused")};
            r.refusalCode = TxRefusals::kStationReceiveOnly;
            return r;
        });
        mox.setMox(true);
        QCOMPARE(refused.count(), 2);
        QCOMPARE(refused.at(1).at(0).value<TxRefusal>(), TxRefusals::stationReceiveOnly());
    }

    void theInterlockWithTheAmpInStandbyOffersOperateAmp()
    {
        MoxController mox;
        TxInterlockPolicy policy;
        policy.setMode(TxInterlockPolicy::Block);
        mox.setInterlockPolicy(&policy);
        mox.onAmpStateChanged(true, false);
        QSignalSpy refused(&mox, &MoxController::moxRefused);
        mox.setMox(true);
        QVERIFY(!mox.isMox());
        QCOMPARE(refused.count(), 1);
        const TxRefusal refusal = refused.at(0).at(0).value<TxRefusal>();
        QCOMPARE(refusal, TxRefusals::ampStandby());
        QCOMPARE(refusal.fix, QByteArray("operateAmp"));
    }

    void theInterlockOverItsSwrLimitRefusesWithSwr()
    {
        MoxController mox;
        TxInterlockPolicy policy;
        policy.setMode(TxInterlockPolicy::Block);
        policy.setSwrGateEnabled(true);
        policy.setSwrGateMax(2.0f);
        mox.setInterlockPolicy(&policy);
        mox.onAmpStateChanged(false, false);
        mox.onAmpSwrUpdated(3.5f);
        QSignalSpy refused(&mox, &MoxController::moxRefused);
        mox.setMox(true);
        QCOMPARE(refused.count(), 1);
        QCOMPARE(refused.at(0).at(0).value<TxRefusal>(), TxRefusals::swr());
    }

    void theKeyingGatesRefusalIsPassedOnAsIs()
    {
        MoxController mox;
        mox.setKeyingGate([](PttMode, const KeyerIdentity&) {
            return KeyingAnswer{KeyingVerdict::Refuse,
                                TxRefusals::otherDeviceHolds(QStringLiteral("Grant's iPhone"))};
        });
        QSignalSpy refused(&mox, &MoxController::moxRefused);
        QSignalSpy rejected(&mox, &MoxController::moxRejected);
        mox.setMox(true);
        QVERIFY(!mox.isMox());
        QCOMPARE(refused.count(), 1);
        QCOMPARE(refused.at(0).at(0).value<TxRefusal>(),
                 TxRefusals::otherDeviceHolds(QStringLiteral("Grant's iPhone")));
        // The desktop's toast reads the same sentence.
        QCOMPARE(rejected.count(), 1);
        QCOMPARE(rejected.at(0).at(0).toString(),
                 QStringLiteral("Grant's iPhone has the transmitter."));
    }
};

QTEST_MAIN(TstTxRefusal)
#include "tst_tx_refusal.moc"
