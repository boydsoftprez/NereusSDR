// no-port-check: test fixture asserts OcMatrix's TX pin action mask and the
// P1 OC byte it reaches; the ported logic and its cites live in OcMatrix.cpp.
//
// The OC Outputs "TX Pin Action mapping" masks the transmit pins against
// MOX, TUNE and two-tone (Thetis Penny.cs adjustForTXAction); receive is
// untouched (adjustForRX with no consider-PA pins). Pin numbers in the
// names are the UI's 1-based pins; the code's are 0-based.

#include <QtTest/QtTest>

#include "core/OcMatrix.h"
#include "core/P1RadioConnection.h"
#include "models/Band.h"

using namespace NereusSDR;

namespace {

using Action = OcMatrix::TXPinAction;

struct Expect {
    Action action;
    int tx;       // maskFor(B20M, true, false, false)
    int tune;     // maskFor(B20M, true, true, false)
    int twoTone;  // maskFor(B20M, true, false, true)
};

// Bank 0 C2 carries the OC byte shifted left one (see tst_p1_oc_matrix_wiring).
int p1OcByte(P1RadioConnection& conn)
{
    quint8 buf[5] = {};
    conn.composeCcForBankForTest(0, buf);
    return int(buf[2]) >> 1;
}

} // namespace

class TestOcMatrixTxPinAction : public QObject {
    Q_OBJECT

private slots:
    void action_masks_pin_1_on_20m_data()
    {
        QTest::addColumn<int>("action");
        QTest::addColumn<int>("tx");
        QTest::addColumn<int>("tune");
        QTest::addColumn<int>("twoTone");
        const Expect rows[] = {
            {Action::Mox,            0x01, 0x00, 0x00},
            {Action::Tune,           0x00, 0x01, 0x00},
            {Action::TwoTone,        0x00, 0x00, 0x01},
            {Action::MoxTune,        0x01, 0x01, 0x00},
            {Action::MoxTwoTone,     0x01, 0x00, 0x01},
            {Action::TuneTwoTone,    0x00, 0x01, 0x01},
            {Action::MoxTuneTwoTone, 0x01, 0x01, 0x01},
        };
        for (const Expect& r : rows) {
            QTest::addRow("action%d", int(r.action))
                << int(r.action) << r.tx << r.tune << r.twoTone;
        }
    }

    void action_masks_pin_1_on_20m()
    {
        QFETCH(int, action);
        QFETCH(int, tx);
        QFETCH(int, tune);
        QFETCH(int, twoTone);

        OcMatrix m;
        m.setPin(Band::Band20m, /*pin=*/0, /*tx=*/true, true);
        m.setPinAction(0, Action(action));

        QCOMPARE(int(m.maskFor(Band::Band20m, true, false, false)), tx);
        QCOMPARE(int(m.maskFor(Band::Band20m, true, true, false)), tune);
        QCOMPARE(int(m.maskFor(Band::Band20m, true, false, true)), twoTone);
        // The two-argument form is tune and two-tone off.
        QCOMPARE(int(m.maskFor(Band::Band20m, true)), tx);
    }

    // Receive ignores the action: RX pin 3 on 40 m is 0x04 whatever it is.
    void receive_ignores_action()
    {
        for (int a = 0; a < int(Action::Count); ++a) {
            OcMatrix m;
            m.setPin(Band::Band40m, /*pin=*/2, /*tx=*/false, true);
            m.setPinAction(2, Action(a));
            for (int t = 0; t < 4; ++t) {
                const bool tune = (t & 1) != 0;
                const bool twoTone = (t & 2) != 0;
                QCOMPARE(int(m.maskFor(Band::Band40m, false, tune, twoTone)), 0x04);
            }
            QCOMPARE(int(m.maskFor(Band::Band40m, false)), 0x04);
        }
    }

    // Default actions (MOX_TUNE_TWOTONE) pass every TX and RX pin through,
    // so every existing caller's byte is unchanged.
    void default_actions_leave_bytes_unchanged()
    {
        OcMatrix m;
        for (int b = 0; b < int(Band::Count); ++b) {
            for (int pin = 0; pin < 7; ++pin) {
                // A pattern that differs per band and between RX and TX.
                m.setPin(Band(b), pin, false, ((b + pin) % 2) == 0);
                m.setPin(Band(b), pin, true, ((b * 3 + pin) % 3) != 0);
            }
        }
        for (int b = 0; b < int(Band::Count); ++b) {
            for (int dir = 0; dir < 2; ++dir) {
                const bool tx = dir == 1;
                int raw = 0;
                for (int pin = 0; pin < 7; ++pin) {
                    if (m.pinEnabled(Band(b), pin, tx)) { raw |= 1 << pin; }
                }
                QCOMPARE(int(m.maskFor(Band(b), tx)), raw);
                QCOMPARE(int(m.maskFor(Band(b), tx, true, false)), raw);
                QCOMPARE(int(m.maskFor(Band(b), tx, false, true)), raw);
                QCOMPARE(int(m.maskFor(Band(b), tx, true, true)), raw);
            }
        }
    }

    // The P1 OC byte reads the connection's TUNE flag while keyed.
    void p1_wire_applies_tune()
    {
        OcMatrix m;
        m.setPin(Band::Band20m, 0, /*tx=*/true, true);   // MOX-only pin
        m.setPin(Band::Band20m, 1, /*tx=*/true, true);   // TUNE-only pin
        m.setPinAction(0, Action::Mox);
        m.setPinAction(1, Action::Tune);

        P1RadioConnection conn(nullptr);
        conn.setBoardForTest(HPSDRHW::Hermes);
        conn.setOcMatrix(&m);
        conn.setReceiverFrequency(0, 14'100'000ULL);
        conn.setTxFrequency(14'100'000ULL);

        conn.setMox(true);
        QCOMPARE(p1OcByte(conn), 0x01);   // voice MOX: the MOX pin only
        conn.setMox(false);

        // TUNE's flag arrives ahead of the key, as RadioModel queues it.
        conn.setOcTxModes(/*tune=*/true, /*twoTone=*/false);
        conn.setMox(true);
        QCOMPARE(p1OcByte(conn), 0x02);   // TUNE: the TUNE pin only
    }

    // A flag that falls while keyed is held until the key ends, so the
    // MOX-only pin does not close in the frames before MOX off.
    void p1_tune_off_held_until_unkey()
    {
        OcMatrix m;
        m.setPin(Band::Band20m, 0, /*tx=*/true, true);
        m.setPinAction(0, Action::Mox);

        P1RadioConnection conn(nullptr);
        conn.setBoardForTest(HPSDRHW::Hermes);
        conn.setOcMatrix(&m);
        conn.setReceiverFrequency(0, 14'100'000ULL);
        conn.setTxFrequency(14'100'000ULL);

        conn.setOcTxModes(true, false);
        conn.setMox(true);
        QCOMPARE(p1OcByte(conn), 0x00);

        conn.setOcTxModes(false, false);  // TUNE ended, MOX off still queued
        QVERIFY(conn.ocTune());
        QCOMPARE(p1OcByte(conn), 0x00);

        conn.setMox(false);
        QVERIFY(!conn.ocTune());
        conn.setMox(true);                // the next voice key
        QCOMPARE(p1OcByte(conn), 0x01);
    }

    // Two-tone rising while keyed applies at once.
    void p1_two_tone_rise_while_keyed_applies()
    {
        OcMatrix m;
        m.setPin(Band::Band20m, 0, /*tx=*/true, true);
        m.setPinAction(0, Action::TwoTone);

        P1RadioConnection conn(nullptr);
        conn.setBoardForTest(HPSDRHW::Hermes);
        conn.setOcMatrix(&m);
        conn.setReceiverFrequency(0, 14'100'000ULL);
        conn.setTxFrequency(14'100'000ULL);

        conn.setMox(true);
        QCOMPARE(p1OcByte(conn), 0x00);
        conn.setOcTxModes(false, true);
        QVERIFY(conn.ocTwoTone());
        QCOMPARE(p1OcByte(conn), 0x01);
    }
};

QTEST_MAIN(TestOcMatrixTxPinAction)
#include "tst_oc_matrix_tx_pin_action.moc"
