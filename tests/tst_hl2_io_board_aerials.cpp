// no-port-check: test-only. Upstream file names appear only in source-cite
// comments that document which upstream line each assertion verifies.
// No upstream logic is ported here; this file is NereusSDR-original.
//
// HL2 I/O board aerial values: the REG_RF_INPUTS (aerial mode) and
// REG_ANTENNA (aerial ports) bytes composed from the antenna selection.
//
// mi0bot Console/HPSDR/Alex.cs UpdateAlexAntSelection [@c26a8a4], its
// HERMESLITE branches:
//   366-372  transverter: rx_only_ant = 1 when the XVTR RX antenna is 4
//            (Alt RX) or TRxAnt is off, else 0
//   424-430  TRxAnt on and no transverter: rx_only_ant = 0
//   445-446  c.SetIOBoardAerialPorts(rx_only_ant, trx_ant - 1, tx_ant - 1, tx)
// mi0bot Console/console.cs:25616-25637 SetIOBoardAerialPorts [@c26a8a4]:
//   IOBoardAerialMode  = rx_only_ant == 1 ? 2 : 0
//   IOBoardAerialPorts = (rx_ant & 0x0f) | (tx_ant << 4)
// and the poll writes them (console.cs:25844-25903): REG_RF_INPUTS (11) on
// steps 3 and 6, REG_ANTENNA (31) on steps 5 and 9, each when it changed.

#include <QtTest/QtTest>

#include <memory>

#include "core/AppSettings.h"
#include "core/IoBoardHl2.h"
#include "core/P1RadioConnection.h"
#include "core/accessories/AlexController.h"
#include "models/Band.h"
#include "models/RadioModel.h"

using namespace NereusSDR;
using Aerials = AlexController::IoBoardAerials;

namespace {

struct Frame {
    int c0, c1, c2, c3, c4;
};

bool isI2c(const quint8 out[5])
{
    const int chip = (out[0] >> 1) & 0x3F;
    return chip == 0x3c || chip == 0x3d;
}

// A P1 connection in the Connected state, so RadioModel's antenna pump
// passes its isConnected() guard.
class ConnectedP1 : public P1RadioConnection {
public:
    ConnectedP1() { setState(ConnectionState::Connected); }
};

QString show(const Aerials& a)
{
    return QStringLiteral("mode %1 ports 0x%2").arg(a.mode).arg(a.ports, 2, 16, QLatin1Char('0'));
}

} // namespace

#define COMPARE_AERIALS(got, wantMode, wantPorts)                                        \
    do {                                                                                 \
        const Aerials g_ = (got);                                                        \
        const Aerials w_ {quint8(wantMode), quint8(wantPorts)};                          \
        QVERIFY2(g_ == w_, qPrintable(show(g_) + QStringLiteral(", want ") + show(w_))); \
    } while (false)

class TestHl2IoBoardAerials : public QObject {
    Q_OBJECT

private:
    // Twelve poll steps (one full UpdateIOBoard cycle) and the I2C writes
    // among the frames they sent, as (register, data).
    static QList<QPair<int, int>> cycleWrites(P1RadioConnection& conn, IoBoardHl2& io)
    {
        QList<QPair<int, int>> writes;
        for (int s = 0; s < 12; ++s) {
            conn.ioBoardPollTickForTest();
            for (int guard = 0; guard < 400 && !io.i2cQueueIsEmpty(); ++guard) {
                quint8 out[5] = {};
                conn.composeNextSubframeForTest(out);
                if (isI2c(out) && out[1] == 0x06) {
                    writes.append({out[3], out[4]});
                }
            }
        }
        return writes;
    }

private slots:
    void init() { AppSettings::instance().clear(); }
    void cleanup() { AppSettings::instance().clear(); }

    // console.cs:25616-25637: the mode is 2 only for rx_only_ant 1; the
    // ports byte is the RX aerial in the low nibble, TX in the high.
    void aerialPortsPackTheBytes()
    {
        COMPARE_AERIALS(AlexController::ioBoardAerialPorts(1, 2, 0), 2, 0x02);
        COMPARE_AERIALS(AlexController::ioBoardAerialPorts(0, 0, 1), 0, 0x10);
        COMPARE_AERIALS(AlexController::ioBoardAerialPorts(2, 1, 2), 0, 0x21);
        COMPARE_AERIALS(AlexController::ioBoardAerialPorts(3, 3, 2), 0, 0x23);
    }

    void defaultsAreAerialOneWithNoAltRx()
    {
        AlexController alex;
        COMPARE_AERIALS(alex.hl2IoBoardAerials(Band::Band20m, Band::Band20m, false), 0, 0x00);
        COMPARE_AERIALS(alex.hl2IoBoardAerials(Band::Band20m, Band::Band20m, true), 0, 0x00);
    }

    // trx_ant - 1 low, tx_ant - 1 high; keyed, trx_ant is the TX aerial.
    void splitAerialsReachTheNibbles()
    {
        AlexController alex;
        alex.setTxAnt(Band::Band20m, 2);
        alex.setRxAnt(Band::Band20m, 3);
        COMPARE_AERIALS(alex.hl2IoBoardAerials(Band::Band20m, Band::Band20m, false), 0, 0x12);
        COMPARE_AERIALS(alex.hl2IoBoardAerials(Band::Band20m, Band::Band20m, true), 0, 0x11);
    }

    // The RX-only aerial 1 selects Alt RX (mode 2); 2 is not an I/O board
    // aerial (mode 0); 3 without a transverter becomes 0 (Alex.cs:375).
    void rxOnlyAerialOneSelectsAltRx()
    {
        AlexController alex;
        alex.setRxOnlyAnt(Band::Band20m, 1);
        COMPARE_AERIALS(alex.hl2IoBoardAerials(Band::Band20m, Band::Band20m, false), 2, 0x00);
        alex.setRxOnlyAnt(Band::Band20m, 2);
        COMPARE_AERIALS(alex.hl2IoBoardAerials(Band::Band20m, Band::Band20m, false), 0, 0x00);
        alex.setRxOnlyAnt(Band::Band20m, 3);
        COMPARE_AERIALS(alex.hl2IoBoardAerials(Band::Band20m, Band::Band20m, false), 0, 0x00);
    }

    // Alex.cs:424-430: receiving on the TX aerial switches the RX-only
    // aerial off, without a transverter.
    void txAerialForReceiveClearsAltRx()
    {
        AlexController alex;
        alex.setTxAnt(Band::Band20m, 2);
        alex.setRxOnlyAnt(Band::Band20m, 1);
        alex.setUseTxAntForRx(true);
        COMPARE_AERIALS(alex.hl2IoBoardAerials(Band::Band20m, Band::Band20m, false), 0, 0x11);
    }

    // Alex.cs:366-372: with a transverter, Alt RX unless TRxAnt is on (the
    // XVTR RX antenna has no NereusSDR setting, so it is the default 0).
    void transverterUsesAltRxUnlessTxAerialReceives()
    {
        AlexController alex;
        COMPARE_AERIALS(alex.hl2IoBoardAerials(Band::XVTR, Band::XVTR, false), 2, 0x00);
        alex.setUseTxAntForRx(true);
        COMPARE_AERIALS(alex.hl2IoBoardAerials(Band::XVTR, Band::XVTR, false), 0, 0x00);

        AlexController active;
        active.setXvtrActive(true);
        COMPARE_AERIALS(active.hl2IoBoardAerials(Band::Band20m, Band::Band20m, false), 2, 0x00);
    }

    // Alex.cs:398-405: rx_out_override on receive makes trx_ant 4, so the
    // low nibble is 3.
    void rxOutOverrideMakesReceiveAerialFour()
    {
        AlexController alex;
        alex.setRxOnlyAnt(Band::Band20m, 1);
        alex.setRxOutOverride(true);
        COMPARE_AERIALS(alex.hl2IoBoardAerials(Band::Band20m, Band::Band20m, false), 2, 0x03);
    }

    // Alex.cs:348-354: keyed, Ext2OutOnTx gives rx_only_ant 1 and
    // Ext1OutOnTx 2; TRxAnt on clears it again (Alex.cs:424-430).
    void transmitExtOutputs()
    {
        AlexController alex;
        alex.setExt2OutOnTx(true);
        COMPARE_AERIALS(alex.hl2IoBoardAerials(Band::Band20m, Band::Band20m, true), 2, 0x00);
        alex.setExt1OutOnTx(true);
        COMPARE_AERIALS(alex.hl2IoBoardAerials(Band::Band20m, Band::Band20m, true), 0, 0x00);
        alex.setExt2OutOnTx(true);
        alex.setUseTxAntForRx(true);
        COMPARE_AERIALS(alex.hl2IoBoardAerials(Band::Band20m, Band::Band20m, true), 0, 0x00);
    }

    // The receive side reads the kept band's aerials; the TX aerial is the
    // current band's.
    void receiveReadsTheKeptBand()
    {
        AlexController alex;
        alex.setRxAnt(Band::Band40m, 3);
        alex.setTxAnt(Band::Band20m, 2);
        COMPARE_AERIALS(alex.hl2IoBoardAerials(Band::Band20m, Band::Band40m, false), 0, 0x12);
    }

    // RadioModel hands the values to the connection on an HL2, and the poll
    // writes them: REG_RF_INPUTS (11) and REG_ANTENNA (31).
    void hl2AntennaChangeReachesTheIoBoardWire()
    {
        auto conn = std::make_unique<ConnectedP1>();
        conn->setBoardForTest(HPSDRHW::HermesLite);
        IoBoardHl2 io;
        io.setDetected(true);
        conn->setIoBoard(&io);

        RadioModel model;
        model.setHpsdrModelForTest(HPSDRModel::HERMESLITE);
        model.injectConnectionForTest(conn.get());

        model.alexControllerMutable().setRxAnt(model.lastBand(), 3);
        model.alexControllerMutable().setRxOnlyAnt(model.lastBand(), 1);
        const auto writes = cycleWrites(*conn, io);
        QVERIFY2(writes.contains(qMakePair(11, 2)), "REG_RF_INPUTS = 2 (Alt RX)");
        QVERIFY2(writes.contains(qMakePair(31, 0x02)), "REG_ANTENNA = 0x02");

        model.alexControllerMutable().setRxOnlyAnt(model.lastBand(), 0);
        model.alexControllerMutable().setTxAnt(model.lastBand(), 2);
        const auto next = cycleWrites(*conn, io);
        QVERIFY2(next.contains(qMakePair(11, 0)), "REG_RF_INPUTS = 0");
        QVERIFY2(next.contains(qMakePair(31, 0x12)), "REG_ANTENNA = 0x12");

        model.injectConnectionForTest(nullptr);
    }

    // Other radios have no I/O board: the antenna pump leaves the values.
    void otherRadiosLeaveTheIoBoardValues()
    {
        auto conn = std::make_unique<ConnectedP1>();
        conn->setBoardForTest(HPSDRHW::HermesLite);
        IoBoardHl2 io;
        io.setDetected(true);
        conn->setIoBoard(&io);

        RadioModel model;
        model.setHpsdrModelForTest(HPSDRModel::ANAN100D);
        model.injectConnectionForTest(conn.get());

        model.alexControllerMutable().setRxAnt(model.lastBand(), 3);
        model.alexControllerMutable().setRxOnlyAnt(model.lastBand(), 1);
        const auto writes = cycleWrites(*conn, io);
        for (const auto& w : writes) {
            QVERIFY2(w.first != 11 && w.first != 31,
                     qPrintable(QStringLiteral("aerial register %1 written").arg(w.first)));
        }

        model.injectConnectionForTest(nullptr);
    }
};

QTEST_MAIN(TestHl2IoBoardAerials)
#include "tst_hl2_io_board_aerials.moc"
