// no-port-check: test-only. Thetis file names appear only in source-cite
// comments that document which upstream line each assertion verifies.
// No Thetis logic is ported here; this file is NereusSDR-original.
//
// =================================================================
// tests/tst_p1_diversity_lock_bit.cpp  (NereusSDR)
// =================================================================
//
// Protocol 1 diversity locks the radio's VFOs with bank 0 C4 bit 7. Thetis
// sets the bit from the diversity state on every UpdateDDCs:
//   From Thetis console.cs:8215-8216 [v2.10.3.15]
//     bool diversity_enabled = Diversity2;
//     if (diversity_enabled) P1_diversity = 1;
//   From Thetis console.cs:8544 [v2.10.3.15]
//     NetworkIO.Protocol1DDCConfig(P1_DDCConfig, P1_diversity, P1_rxcount, nddc);
//   From Thetis ChannelMaster/networkproto1.c:471 [v2.10.3.15]
//     C4 |= (P1_en_diversity) << 7;		// if diversity, locks VFOs
// and on the Orion class (nddc 5) frame slots 0 and 1 both carry RX1's
// frequency, slot 1 through bank 3:
//   From Thetis ChannelMaster/networkproto1.c:504-505 [v2.10.3.15]
//     else if (nddc == 5)
//         ddc_freq = prn->rx[0].frequency;
//
// NereusSDR computed the flag (DdcAssignment::p1Diversity) and never sent
// it: P1RadioConnection's diversity bit had no writer, so the radio ran the
// pair unlocked.
// =================================================================

#include <QtTest/QtTest>

#include "core/AppSettings.h"
#include "core/BoardCapabilities.h"
#include "core/P1RadioConnection.h"
#include "core/ReceiverManager.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

constexpr double k20mHz = 14200000.0;
constexpr double k40mHz =  7100000.0;

// setState is protected on RadioConnection; the model pushes only to a
// connection that reports Connected.
class ConnectedP1 final : public P1RadioConnection {
public:
    ConnectedP1() { setState(ConnectionState::Connected); }
};

struct DetachConnection {
    RadioModel* model{nullptr};
    ~DetachConnection() { if (model) { model->injectConnectionForTest(nullptr); } }
};

QByteArray bank(const P1RadioConnection& conn, int idx)
{
    quint8 out[5] = {};
    conn.composeCcForBankForTest(idx, out);
    return QByteArray(reinterpret_cast<const char*>(out), 5);
}

quint32 bankFrequency(const P1RadioConnection& conn, int idx)
{
    const QByteArray b = bank(conn, idx);
    return (quint32(quint8(b[1])) << 24) | (quint32(quint8(b[2])) << 16)
         | (quint32(quint8(b[3])) << 8)  |  quint32(quint8(b[4]));
}

bool lockBit(const P1RadioConnection& conn)
{
    return (quint8(bank(conn, 0)[4]) & 0x80) != 0;
}

// A Protocol 1 session on `board` running `radio`, with the production
// wiring the bank bytes depend on: the model applied as connectToRadio does
// (applyHpsdrModel, which also tells ReceiverManager), ReceiverManager's
// frequency pushes, and its DDC configuration (Protocol1DDCConfig's nddc and
// rx count) into the connection, as RadioModel::wireConnectionSignals
// connects them.
struct Session {
    Session(HPSDRHW board, HPSDRModel radio)
    {
        AppSettings::instance().clear();
        model.setBoardForTest(board);
        model.setHpsdrModelForTest(radio);
        conn.setBoardForTest(board);
        model.injectConnectionForTest(&conn);
        detach.model = &model;
        model.wireReceiverManagerHardwarePushesForTest();
        QObject::connect(model.receiverManager(), &ReceiverManager::ddcConfigChanged,
                         &conn, &P1RadioConnection::applyPsDdcConfig);
        model.receiverManager()->setP1Codec(conn.p1Codec());

        model.configureStreamPool(4, 4, 192000);
        for (int i = 0; i < 4; ++i) {
            model.receiverManager()->createReceiver();
        }
    }
    ~Session() { AppSettings::instance().clear(); }

    int add(double hz)
    {
        const int id = model.addSlice();
        model.sliceById(id)->setFrequency(hz);
        return id;
    }

    RadioModel       model;
    ConnectedP1      conn;
    DetachConnection detach;
};

} // namespace

class TestP1DiversityLockBit : public QObject {
    Q_OBJECT

private slots:
    void lock_bit_follows_diversity_and_both_slots_carry_rx1_data()
    {
        QTest::addColumn<int>("board");
        QTest::addColumn<int>("radio");
        QTest::newRow("ANAN-100D") << int(HPSDRHW::Angelia)   << int(HPSDRModel::ANAN100D);
        QTest::newRow("ANAN-7000D") << int(HPSDRHW::OrionMKII) << int(HPSDRModel::ANAN7000D);
    }
    void lock_bit_follows_diversity_and_both_slots_carry_rx1()
    {
        QFETCH(int, board);
        QFETCH(int, radio);
        Session s{HPSDRHW(board), HPSDRModel(radio)};
        QVERIFY(s.conn.p1Codec() != nullptr);

        const int a = s.add(k20mHz);
        s.add(k40mHz);
        QCOMPARE(a, 0);
        QVERIFY(!lockBit(s.conn));

        s.model.sliceById(a)->setDiversityEnabled(true);
        QVERIFY(s.model.diversityActive());
        QVERIFY2(lockBit(s.conn), "bank 0 C4 bit 7 must lock the VFOs under diversity");
        QCOMPARE(bankFrequency(s.conn, 2), quint32(k20mHz));
        QCOMPARE(bankFrequency(s.conn, 3), quint32(k20mHz));

        // A retune under diversity moves both slots.
        s.model.sliceById(a)->setFrequency(k20mHz + 5000.0);
        QCOMPARE(bankFrequency(s.conn, 2), quint32(k20mHz + 5000.0));
        QCOMPARE(bankFrequency(s.conn, 3), quint32(k20mHz + 5000.0));

        s.model.sliceById(a)->setDiversityEnabled(false);
        QVERIFY(!s.model.diversityActive());
        QVERIFY2(!lockBit(s.conn), "bank 0 C4 bit 7 must clear when diversity stops");

        // Closing slice A ends diversity too.
        s.model.sliceById(a)->setDiversityEnabled(true);
        QVERIFY(lockBit(s.conn));
        s.model.removeSlice(a);
        QVERIFY(!s.model.diversityActive());
        QVERIFY2(!lockBit(s.conn), "bank 0 C4 bit 7 must clear when slice A closes");
    }
};

QTEST_MAIN(TestP1DiversityLockBit)
#include "tst_p1_diversity_lock_bit.moc"
