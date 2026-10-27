// no-port-check: NereusSDR-original test. Thetis and mi0bot file names
// appear only as cites for the behavior asserted.
// =================================================================
// tests/tst_p1_cw_key_overlay.cpp  (NereusSDR)
// =================================================================
//
// The Protocol 1 host key bits on the wire (R-CW-12, R-CW-15). Frames are
// built by the EP2 sender's own pass (serviceEp2SendForTest ->
// composeCommandFrame), with no radio socket:
//   - CW enable 1 and key bits 5: every I and Q word of both zones is
//     0x0005 (Thetis networkproto1.c:738-741 [v2.10.3.15]);
//   - CW enable 0: the zones are byte-identical to fillTxZone's output;
//   - the transmit cut: the words are 0 and bank 13 C1 is 0 at once, and
//     the cut's own unkey puts bank 13 on a following frame;
//   - the HL2 store rule (mi0bot netInterface.c:1193-1224
//     [v2.10.3.13-beta2]): CWX counts only with break-in, MOX or CWX PTT;
//     CWX PTT is bit 3 (mi0bot networkproto1.c:1247-1252);
//   - a changed CW enable puts bank 13 on the next frame;
//   - with CW off and key bits held, the HL2 low-bit mask (deskhpsdr
//     old_protocol.c:2441-2453 [@120188f]) still reaches the frame.
// Nothing here keys a radio.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-26: CW transmit, the P1 key overlay tests. J.J. Boyd
//               (KG4VCF), AI-assisted via Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QHostAddress>

#include <vector>

#include "core/P1RadioConnection.h"

using namespace NereusSDR;

namespace {

constexpr int kZoneOffsets[2] = {16, 528};
constexpr int kSamplesPerZone = 63;
constexpr int kBytesPerSample = 8;
constexpr qint64 kFrameNs = 2625000;   // P1RadioConnection::kEp2PacketIntervalUs

// One EP2 frame per pass of the sender's own compose path.
class Ep2Seam {
public:
    explicit Ep2Seam(P1RadioConnection& conn) : m_conn(conn)
    {
        m_conn.resetEp2PacerForTest();
        QList<QByteArray> none;
        m_conn.serviceEp2SendForTest(0, &none);   // starts the pacer clock
    }
    QByteArray next()
    {
        QList<QByteArray> out;
        m_t += kFrameNs;
        m_conn.serviceEp2SendForTest(m_t, &out);
        return out.size() == 1 ? out.first() : QByteArray();
    }
private:
    P1RadioConnection& m_conn;
    qint64 m_t{0};
};

// The EP2 pacer sends only on a running, connected link. The frames go to
// the seam's list; the discard port on this host is never written.
void setUp(P1RadioConnection& conn, HPSDRHW board)
{
    conn.init();
    conn.setBoardForTest(board);
    conn.armEp2ForTest(QHostAddress::LocalHost, 9);
}

void queueTxIq(P1RadioConnection& conn, float value)
{
    std::vector<float> iq(126 * 2, value);
    conn.sendTxIq(iq.data(), 126);
}

// Every I and Q word of both zones equals word.
bool allIqWordsAre(const QByteArray& frame, quint16 word)
{
    if (frame.size() != 1032) {
        return false;
    }
    for (const int zone : kZoneOffsets) {
        for (int i = 0; i < kSamplesPerZone; ++i) {
            const int base = zone + i * kBytesPerSample + 4;
            for (int w = 0; w < 2; ++w) {
                const quint16 v = quint16((quint8(frame[base + 2 * w]) << 8)
                                          | quint8(frame[base + 2 * w + 1]));
                if (v != word) {
                    qWarning() << "zone" << zone << "sample" << i << "word" << w
                               << "is" << Qt::hex << v << "expected" << word;
                    return false;
                }
            }
        }
    }
    return true;
}

QByteArray zones(const QByteArray& frame)
{
    return frame.mid(16, 504) + frame.mid(528, 504);
}

CwWireState cwOn(bool breakIn = false)
{
    CwWireState s;
    s.enable = true;
    s.breakIn = breakIn;
    return s;
}

} // namespace

class TestP1CwKeyOverlay : public QObject {
    Q_OBJECT
private slots:
    // Key bits 5 (dot and CWX) replace every I and Q word, TX samples
    // queued or not. On the HL2 break-in is on, so its store rule keeps CWX.
    void enableOneKeyFiveFillsEveryIqWord_data()
    {
        QTest::addColumn<int>("board");
        QTest::newRow("Hermes") << int(HPSDRHW::Hermes);
        QTest::newRow("HermesLite") << int(HPSDRHW::HermesLite);
    }
    void enableOneKeyFiveFillsEveryIqWord()
    {
        QFETCH(int, board);
        P1RadioConnection conn;
        setUp(conn, HPSDRHW(board));
        Ep2Seam seam(conn);
        conn.setCwWireState(cwOn(/*breakIn=*/true));
        conn.cwKeyLine()->setDot(true);
        conn.cwKeyLine()->setCwx(true);

        queueTxIq(conn, 0.5f);
        const QByteArray withSamples = seam.next();
        QVERIFY(allIqWordsAre(withSamples, 0x0005));
        // L/R bytes are not touched (no radio audio queued: zero).
        for (const int zone : kZoneOffsets) {
            for (int i = 0; i < kSamplesPerZone; ++i) {
                for (int b = 0; b < 4; ++b) {
                    QCOMPARE(int(quint8(withSamples[zone + i * kBytesPerSample + b])), 0);
                }
            }
        }
        // In receive with nothing queued too.
        QVERIFY(allIqWordsAre(seam.next(), 0x0005));
    }

    // CW enable 0 with key bits set: the zones are fillTxZone's, byte for
    // byte (the HL2 row keeps its low-bit mask, which fillTxZone carries).
    void enableZeroLeavesFillTxZoneBytes_data()
    {
        QTest::addColumn<int>("board");
        QTest::newRow("Hermes") << int(HPSDRHW::Hermes);
        QTest::newRow("HermesLite") << int(HPSDRHW::HermesLite);
    }
    void enableZeroLeavesFillTxZoneBytes()
    {
        QFETCH(int, board);
        // Odd low bytes, so the HL2 mask shows.
        constexpr float kValue = 3.0f / 32767.0f;

        P1RadioConnection reference;
        setUp(reference, HPSDRHW(board));
        std::vector<float> iq(126 * 2, kValue);
        const QByteArray expected = reference.sendTxIqAndCapture(iq.data(), 126);

        P1RadioConnection conn;
        setUp(conn, HPSDRHW(board));
        Ep2Seam seam(conn);
        CwWireState off;
        off.breakIn = true;
        conn.setCwWireState(off);
        conn.cwKeyLine()->setDot(true);
        conn.cwKeyLine()->setDash(true);
        conn.cwKeyLine()->setCwx(true);
        queueTxIq(conn, kValue);
        const QByteArray frame = seam.next();
        QCOMPARE(frame.size(), 1032);
        QCOMPARE(zones(frame), zones(expected));
        const quint8 iLo = quint8(frame[16 + 5]);
        if (HPSDRHW(board) == HPSDRHW::HermesLite) {
            QCOMPARE(int(iLo & 0x01), 0);
        } else {
            QCOMPARE(int(iLo & 0x01), 1);
        }
    }

    // The safety rule: under the transmit cut the words are 0 and bank 13
    // C1 reads 0 at once, before the cut's queued unkey runs; after it
    // runs, bank 13 is forced onto a following frame with C1 0.
    void transmitCutZeroesKeyBitsAndCwEnable()
    {
        P1RadioConnection conn;
        setUp(conn, HPSDRHW::Hermes);
        Ep2Seam seam(conn);
        conn.setCwWireState(cwOn());
        conn.cwKeyLine()->setDot(true);
        conn.cwKeyLine()->setCwx(true);
        // Past the enable's own bank 13 flush.
        for (int f = 0; f < 4; ++f) {
            QVERIFY(allIqWordsAre(seam.next(), 0x0005));
        }
        QVERIFY(!conn.forceBank13NextForTest());
        QCOMPARE(int(quint8(conn.captureBank13ForTest()[1])), 1);

        // The cut's own log line, expected.
        QTest::ignoreMessage(QtWarningMsg, "Transmit cut: MOX and the T/R relay off");
        conn.cutTransmit();
        QVERIFY(conn.transmitCut());
        // The queued unkey has not run yet.
        QVERIFY(!conn.forceBank13NextForTest());
        queueTxIq(conn, 0.5f);
        QVERIFY(allIqWordsAre(seam.next(), 0x0000));
        QCOMPARE(int(quint8(conn.captureBank13ForTest()[1])), 0);

        QCoreApplication::processEvents();   // applyTransmitCut
        QVERIFY(conn.forceBank13NextForTest());
        bool sawBank13 = false;
        for (int f = 0; f < 3 && !sawBank13; ++f) {
            const QByteArray frame = seam.next();
            QVERIFY(allIqWordsAre(frame, 0x0000));
            for (const int cc : {11, 523}) {
                if ((quint8(frame[cc]) & 0xFE) == 0x1E) {
                    sawBank13 = true;
                    QCOMPARE(int(quint8(frame[cc + 1])), 0);
                }
            }
        }
        QVERIFY(sawBank13);

        conn.clearTransmitCut();
        QVERIFY(allIqWordsAre(seam.next(), 0x0005));
        QCOMPARE(int(quint8(conn.captureBank13ForTest()[1])), 1);
    }

    // mi0bot stores CWX only while break-in, MOX or CWX PTT is on; CWX PTT
    // is bit 3. HL2 only: the Hermes row sends CWX whatever the state.
    void hl2StoreRule_data()
    {
        QTest::addColumn<int>("board");
        QTest::addColumn<bool>("breakIn");
        QTest::addColumn<bool>("mox");
        QTest::addColumn<bool>("cwxPtt");
        QTest::addColumn<int>("word");
        QTest::newRow("HL2 none")    << int(HPSDRHW::HermesLite) << false << false << false << 0x00;
        QTest::newRow("HL2 cwxPtt")  << int(HPSDRHW::HermesLite) << false << false << true  << 0x09;
        QTest::newRow("HL2 breakIn") << int(HPSDRHW::HermesLite) << true  << false << false << 0x01;
        QTest::newRow("HL2 mox")     << int(HPSDRHW::HermesLite) << false << true  << false << 0x01;
        QTest::newRow("Hermes none") << int(HPSDRHW::Hermes)     << false << false << false << 0x01;
        QTest::newRow("Hermes cwxPtt ignored") << int(HPSDRHW::Hermes) << false << false << true << 0x01;
    }
    void hl2StoreRule()
    {
        QFETCH(int, board);
        QFETCH(bool, breakIn);
        QFETCH(bool, mox);
        QFETCH(bool, cwxPtt);
        QFETCH(int, word);
        P1RadioConnection conn;
        setUp(conn, HPSDRHW(board));
        Ep2Seam seam(conn);
        conn.setCwWireState(cwOn(breakIn));
        if (mox) {
            conn.setMox(true);
        }
        conn.cwKeyLine()->setCwx(true);
        conn.cwKeyLine()->setCwxPtt(cwxPtt);
        QVERIFY(allIqWordsAre(seam.next(), quint16(word)));
        if (mox) {
            conn.setMox(false);
        }
    }

    // A changed CW enable puts bank 13 on the next frame; the same state
    // again does not.
    void enableChangePutsBank13Next()
    {
        P1RadioConnection conn;
        setUp(conn, HPSDRHW::Hermes);
        Ep2Seam seam(conn);
        for (int f = 0; f < 4; ++f) {
            QCOMPARE(seam.next().size(), 1032);
        }
        QVERIFY(!conn.forceBank13NextForTest());

        conn.setCwWireState(cwOn());
        QVERIFY(conn.forceBank13NextForTest());
        QByteArray frame = seam.next();
        QCOMPARE(int(quint8(frame[11])), 0x1E);
        QCOMPARE(int(quint8(frame[12])), 1);
        QVERIFY(!conn.forceBank13NextForTest());

        CwWireState again = cwOn();
        again.speedWpm = 30;   // a change, but not of the enable
        conn.setCwWireState(again);
        QVERIFY(!conn.forceBank13NextForTest());

        conn.setCwWireState(CwWireState{});
        QVERIFY(conn.forceBank13NextForTest());
        frame = seam.next();
        QCOMPARE(int(quint8(frame[11])), 0x1E);
        QCOMPARE(int(quint8(frame[12])), 0);
    }

    // CW off, key bits held: the zones carry the TX I/Q as sendTxIq wrote
    // it, so the HL2 clears I lo and Q lo bit 0 and the Hermes keeps it.
    void hl2LowBitMaskOutsideCw_data()
    {
        QTest::addColumn<int>("board");
        QTest::addColumn<int>("loBit");
        QTest::newRow("HermesLite") << int(HPSDRHW::HermesLite) << 0;
        QTest::newRow("Hermes") << int(HPSDRHW::Hermes) << 1;
    }
    void hl2LowBitMaskOutsideCw()
    {
        QFETCH(int, board);
        QFETCH(int, loBit);
        P1RadioConnection conn;
        setUp(conn, HPSDRHW(board));
        Ep2Seam seam(conn);
        conn.cwKeyLine()->setDot(true);
        conn.cwKeyLine()->setCwx(true);
        conn.cwKeyLine()->setCwxPtt(true);
        queueTxIq(conn, 3.0f / 32767.0f);   // I = Q = 3: odd low bytes
        QVERIFY(allIqWordsAre(seam.next(), quint16(0x0002 | loBit)));
    }
};

QTEST_GUILESS_MAIN(TestP1CwKeyOverlay)
#include "tst_p1_cw_key_overlay.moc"
