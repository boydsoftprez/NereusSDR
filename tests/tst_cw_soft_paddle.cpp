// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-05 - J.J. Boyd (KG4VCF), hot keys plan Task 5.
//                AI-assisted implementation via Anthropic Claude Code.
// no-port-check: NereusSDR-original test.
// The key line's second pair of bits and the soft paddle that drives them.
#include <QtTest/QtTest>

#include <QSignalSpy>

#include "core/CwKeyLine.h"
#include "core/CwSoftPaddle.h"
#include "core/safety/TxRefusal.h"

using namespace NereusSDR;

namespace {

TxRefusal shutRefusal()
{
    TxRefusal refusal;
    refusal.code = QByteArrayLiteral("keyerOff");
    refusal.text = QStringLiteral("The keyer is off.");
    return refusal;
}

} // namespace

class TestCwSoftPaddle final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() { qRegisterMetaType<NereusSDR::TxRefusal>(); }

    // ---- The line ----

    void serialAndSoftNeverClearEachOther()
    {
        CwKeyLine line;
        line.setDot(true);
        line.setSoftDot(true);
        line.setSoftDot(false);
        QVERIFY(line.dot());
        QCOMPARE(int(line.p1Bits(false)), 0x04);

        line.setDot(false);
        QVERIFY(!line.dot());
        QCOMPARE(int(line.p1Bits(false)), 0);

        line.setSoftDash(true);
        line.setDash(true);
        line.setDash(false);
        QVERIFY(line.dash());
        QCOMPARE(int(line.p1Bits(false)), 0x02);
        line.setSoftDash(false);
        QVERIFY(!line.dash());
        QCOMPARE(int(line.p1Bits(true)), 0);
    }

    void softBitsStayInsideTheMasks()
    {
        CwKeyLine line;
        line.setSoftDot(true);
        line.setSoftDash(true);
        QCOMPARE(int(line.p1Bits(false)), 0x06);
        QCOMPARE(int(line.p1Bits(true)), 0x06);
        QCOMPARE(line.p1Bits(false) & ~0x07, 0);
        QCOMPARE(line.p1Bits(true) & ~0x0F, 0);
        // They are dot and dash and nothing else.
        QVERIFY(!line.cwx());
        QVERIFY(!line.cwxPtt());

        line.setCwx(true);
        line.setCwxPtt(true);
        QCOMPARE(int(line.p1Bits(false)), 0x07);
        QCOMPARE(int(line.p1Bits(true)), 0x0F);
        QCOMPARE(line.p1Bits(true) & ~0x0F, 0);
    }

    void onChangeFiresOncePerRealChange()
    {
        CwKeyLine line;
        int changes = 0;
        line.setOnChange([&changes]() { ++changes; });

        line.setSoftDot(true);
        QCOMPARE(changes, 1);
        line.setSoftDot(true);      // no change at all
        QCOMPARE(changes, 1);
        line.setDot(true);          // the wire already says dot
        QCOMPARE(changes, 1);
        line.setSoftDot(false);     // serial still holds it
        QCOMPARE(changes, 1);
        line.setDot(false);
        QCOMPARE(changes, 2);

        line.setDash(true);
        QCOMPARE(changes, 3);
        line.setSoftDash(true);
        QCOMPARE(changes, 3);
        line.setDash(false);        // soft still holds it
        QCOMPARE(changes, 3);
        line.setSoftDash(false);
        QCOMPARE(changes, 4);
        line.setSoftDash(false);
        QCOMPARE(changes, 4);
    }

    // ---- The paddle ----

    void anOpenGateKeysAndReleaseClears()
    {
        CwKeyLine line;
        CwSoftPaddle paddle;
        paddle.setLine(&line);
        paddle.setGateAllowed(true);
        QSignalSpy refused(&paddle, &CwSoftPaddle::keyRefused);

        paddle.setDot(true);
        QVERIFY(line.dot());
        QVERIFY(paddle.isKeying());
        paddle.setDash(true);
        QVERIFY(line.dash());
        paddle.setDot(false);
        QVERIFY(!line.dot());
        QVERIFY(line.dash());
        paddle.setDash(false);
        QVERIFY(!line.dash());
        QVERIFY(!paddle.isKeying());
        QCOMPARE(refused.size(), 0);
    }

    void aShutGateBlocksUntilAFreshPressAndSaysSoOnce()
    {
        CwKeyLine line;
        CwSoftPaddle paddle;
        paddle.setLine(&line);
        paddle.setGateAllowed(false, shutRefusal());
        QSignalSpy refused(&paddle, &CwSoftPaddle::keyRefused);

        paddle.setDot(true);
        QVERIFY(!line.dot());
        QCOMPARE(refused.size(), 1);
        QCOMPARE(refused.first().first().value<TxRefusal>().text, shutRefusal().text);
        paddle.setDot(true);        // a repeat of the same press
        QCOMPARE(refused.size(), 1);

        // The gate opening does not key what is still held.
        paddle.setGateAllowed(true);
        paddle.setDot(true);
        QVERIFY(!line.dot());
        QCOMPARE(refused.size(), 1);

        paddle.setDot(false);
        paddle.setDot(true);
        QVERIFY(line.dot());
        QCOMPARE(refused.size(), 1);
        paddle.setDot(false);

        // A second press with the gate shut says it again.
        paddle.setGateAllowed(false, shutRefusal());
        paddle.setDot(true);
        QCOMPARE(refused.size(), 2);
    }

    void aGateGoingFalseReleasesWhatIsDown()
    {
        CwKeyLine line;
        CwSoftPaddle paddle;
        paddle.setLine(&line);
        paddle.setGateAllowed(true);
        QSignalSpy refused(&paddle, &CwSoftPaddle::keyRefused);
        paddle.setDot(true);
        paddle.setDash(true);

        paddle.setGateAllowed(false, shutRefusal());
        QCOMPARE(int(line.p1Bits(true)), 0);
        QVERIFY(!paddle.isKeying());
        QCOMPARE(refused.size(), 1);

        // Still held, gate open again: nothing until each comes up.
        paddle.setGateAllowed(true);
        paddle.setDot(true);
        paddle.setDash(true);
        QCOMPARE(int(line.p1Bits(true)), 0);
        QCOMPARE(refused.size(), 1);
        paddle.setDash(false);
        paddle.setDash(true);
        QVERIFY(line.dash());
        QVERIFY(!line.dot());

        // Nothing down: a shut gate has nothing to say.
        paddle.setDash(false);
        paddle.setDot(false);
        paddle.setGateAllowed(false, shutRefusal());
        QCOMPARE(refused.size(), 1);
    }

    void releaseKeysClearsBothAndBlocksTheHeldKeys()
    {
        CwKeyLine line;
        CwSoftPaddle paddle;
        paddle.setLine(&line);
        paddle.setGateAllowed(true);
        paddle.setDot(true);
        paddle.setDash(true);

        paddle.releaseKeys();
        QCOMPARE(int(line.p1Bits(true)), 0);
        paddle.setDot(true);
        QVERIFY(!line.dot());
        paddle.setDot(false);
        paddle.setDot(true);
        QVERIFY(line.dot());

        // With nothing down it blocks nothing.
        paddle.setDot(false);
        paddle.setDash(false);
        paddle.releaseKeys();
        paddle.setDash(true);
        QVERIFY(line.dash());
    }

    void withNoLineAPressBlocks()
    {
        CwSoftPaddle paddle;
        paddle.setGateAllowed(true);
        QSignalSpy refused(&paddle, &CwSoftPaddle::keyRefused);
        paddle.setDot(true);
        QVERIFY(!paddle.isKeying());
        QCOMPARE(refused.size(), 0);

        // A line arriving under the held key does not key it.
        CwKeyLine line;
        paddle.setLine(&line);
        paddle.setDot(true);
        QVERIFY(!line.dot());
        paddle.setDot(false);
        paddle.setDot(true);
        QVERIFY(line.dot());
    }

    void aNewLineLetsGoOfTheOld()
    {
        CwKeyLine first;
        CwKeyLine second;
        CwSoftPaddle paddle;
        paddle.setLine(&first);
        paddle.setGateAllowed(true);
        paddle.setDot(true);
        QVERIFY(first.dot());

        paddle.setLine(&first);     // the same line again changes nothing
        QVERIFY(first.dot());

        paddle.setLine(&second);
        QVERIFY(!first.dot());
        QVERIFY(!second.dot());
        paddle.setDot(true);
        QVERIFY(!second.dot());
        paddle.setDot(false);
        paddle.setDot(true);
        QVERIFY(second.dot());
        QVERIFY(!first.dot());
    }

    void goingAwayLetsGo()
    {
        CwKeyLine line;
        {
            CwSoftPaddle paddle;
            paddle.setLine(&line);
            paddle.setGateAllowed(true);
            paddle.setDash(true);
            QVERIFY(line.dash());
        }
        QVERIFY(!line.dash());
    }
};

QTEST_MAIN(TestCwSoftPaddle)
#include "tst_cw_soft_paddle.moc"
