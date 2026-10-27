// no-port-check: NereusSDR-original test.
// =================================================================
// tests/tst_cw_keyer_gate.cpp  (NereusSDR)
// =================================================================
//
// CW transmit (R-CW-05, D16): the keyer gate's truth table. Each input
// alone, with every other passing, refuses with its own words; the first
// failing input in the gate's order wins; all passing allows. The PC
// microphone is not an input.
//
// Modification history (NereusSDR):
//   2026-10-26: CW transmit, the keyer gate's truth table. J.J. Boyd
//               (KG4VCF), AI-assisted via Claude Code.
//   2026-10-26: CW transmit early fix wave M5: the not-CW code.
//               J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <functional>

#include "OperatorWording.h"
#include "core/safety/CwKeyerGate.h"
#include "core/safety/TxRefusal.h"
#include "core/station/StationRadios.h"

using namespace NereusSDR;
using namespace NereusSDR::safety;

namespace {

CwKeyerGateInputs passing()
{
    CwKeyerGateInputs in;
    in.hasTxSlice = true;
    in.txSliceInCw = true;
    in.bandPlan = MoxCheckResult{true, QString()};
    return in;
}

struct Row {
    const char* name;
    std::function<void(CwKeyerGateInputs&)> fail;
    TxRefusal expected;
};

QList<Row> rowsInGateOrder()
{
    MoxCheckResult outside{false, QStringLiteral("14.350001 MHz is outside the transmit bands for "
                                                 "your region (United States).")};
    return {
        {"noTxSlice", [](CwKeyerGateInputs& in) { in.hasTxSlice = false; },
         TxRefusals::noTransmitSlice()},
        {"notCw", [](CwKeyerGateInputs& in) { in.txSliceInCw = false; },
         // Early fix wave M5: notReady, not the band plan's code.
         TxRefusal{TxRefusals::kNotReady, QString::fromLatin1(kCwGateNotCwText), {}}},
        {"receiveOnly", [](CwKeyerGateInputs& in) { in.receiveOnly = true; },
         TxRefusals::stationReceiveOnly()},
        {"txInhibited", [](CwKeyerGateInputs& in) { in.txInhibited = true; },
         TxRefusals::txInhibited()},
        {"paTripped", [](CwKeyerGateInputs& in) { in.paTripped = true; },
         TxRefusals::paProtection()},
        {"radioLinkDown", [](CwKeyerGateInputs& in) { in.radioLinkDown = true; },
         TxRefusals::radioLinkDown()},
        {"radioChange", [](CwKeyerGateInputs& in) { in.radioChangeUnderway = true; },
         TxRefusal{TxRefusals::kNotReady, StationRadios::switchingReason(), {}}},
        {"stopLatched", [](CwKeyerGateInputs& in) { in.stopLatched = true; },
         TxRefusal{TxRefusals::kInterlock, QString::fromLatin1(kCwGateStopLatchedText), {}}},
        {"bandPlan", [outside](CwKeyerGateInputs& in) { in.bandPlan = outside; },
         TxRefusals::bandPlan(outside.reason)},
        {"interlock",
         [](CwKeyerGateInputs& in) { in.interlockRefusal = TxRefusals::ampStandby().text; },
         TxRefusals::ampStandby()},
        {"interlockNotGranted",
         [](CwKeyerGateInputs& in) {
             in.interlockArmed = true;
             in.interlockGranted = false;
         },
         TxRefusal{TxRefusals::kInterlock, QString::fromLatin1(kCwGateInterlockNotGrantedText), {}}},
    };
}

} // namespace

class TestCwKeyerGate : public QObject {
    Q_OBJECT

private slots:
    void allPassingAllows()
    {
        const CwKeyerGateResult r = evaluateCwKeyerGate(passing());
        QVERIFY(r.allowed);
        QVERIFY(r.refusal.isEmpty());
    }

    void defaultInputsRefuse()
    {
        // A default-constructed input has no transmit slice.
        const CwKeyerGateResult r = evaluateCwKeyerGate(CwKeyerGateInputs{});
        QVERIFY(!r.allowed);
        QCOMPARE(r.refusal, TxRefusals::noTransmitSlice());
    }

    void eachInputAloneRefusesWithItsOwnWords()
    {
        for (const Row& row : rowsInGateOrder()) {
            CwKeyerGateInputs in = passing();
            row.fail(in);
            const CwKeyerGateResult r = evaluateCwKeyerGate(in);
            QVERIFY2(!r.allowed, row.name);
            QVERIFY2(r.refusal == row.expected,
                     qPrintable(QStringLiteral("%1: %2 / %3").arg(QLatin1String(row.name),
                                                                  QString::fromLatin1(r.refusal.code),
                                                                  r.refusal.text)));
            QVERIFY2(OperatorWording::isPlain(r.refusal.text), qPrintable(r.refusal.text));
        }
    }

    void theEarlierInputWins()
    {
        const QList<Row> rows = rowsInGateOrder();
        for (int i = 0; i < rows.size(); ++i) {
            for (int j = i + 1; j < rows.size(); ++j) {
                CwKeyerGateInputs in = passing();
                rows.at(j).fail(in);
                rows.at(i).fail(in);
                const CwKeyerGateResult r = evaluateCwKeyerGate(in);
                QVERIFY(!r.allowed);
                QVERIFY2(r.refusal == rows.at(i).expected,
                         qPrintable(QStringLiteral("%1 before %2").arg(
                             QLatin1String(rows.at(i).name), QLatin1String(rows.at(j).name))));
            }
        }
    }

    void anArmedInterlockThatGrantedAllows()
    {
        CwKeyerGateInputs in = passing();
        in.interlockArmed = true;
        in.interlockGranted = true;
        QVERIFY(evaluateCwKeyerGate(in).allowed);
        // A grant with nothing armed changes nothing.
        in.interlockArmed = false;
        QVERIFY(evaluateCwKeyerGate(in).allowed);
    }

    void interlockRefusalsKeepTheirCode()
    {
        CwKeyerGateInputs in = passing();
        in.interlockRefusal = TxRefusals::swr().text;
        QCOMPARE(evaluateCwKeyerGate(in).refusal, TxRefusals::swr());
        in.interlockRefusal = TxRefusals::interlock().text;
        QCOMPARE(evaluateCwKeyerGate(in).refusal, TxRefusals::interlock());
        in.interlockRefusal = TxRefusals::ampStandby().text;
        QCOMPARE(evaluateCwKeyerGate(in).refusal.fix, QByteArray(TxRefusals::kFixOperateAmp));
    }

    void aBandPlanRefusalWithItsOwnCodeKeepsIt()
    {
        CwKeyerGateInputs in = passing();
        in.bandPlan = MoxCheckResult{false, QStringLiteral("The transmit region setting is invalid.")};
        QCOMPARE(evaluateCwKeyerGate(in).refusal.code, QByteArray(TxRefusals::kBandPlan));
        in.bandPlan.refusalCode = TxRefusals::kNoTransmitSlice;
        in.bandPlan.reason = TxRefusals::noTransmitSlice().text;
        QCOMPARE(evaluateCwKeyerGate(in).refusal, TxRefusals::noTransmitSlice());
    }
};

QTEST_GUILESS_MAIN(TestCwKeyerGate)
#include "tst_cw_keyer_gate.moc"
