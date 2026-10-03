// no-port-check: test-only. Thetis file names appear only in source-cite
// comments that document which upstream line each assertion verifies.
// No Thetis logic is ported here; this file is NereusSDR-original.
//
// =================================================================
// Protocol 1 band outputs follow the transmitting slice while keyed,
// and the receiving slice's VFO while not.
// =================================================================
//
// Plan Task 14 (3M-1 transmit, Phase 3F section 16.3.2, R-R3-49).
//
// The OC outputs (bank 0 C2) select a band's pins. Thetis picks the band
// from the VFO that is transmitting while keyed, and from VFO A while not:
//   From Thetis HPSDR/Penny.cs:174-177 [v2.10.3.15]
//     if (tx && VFOBTX)
//         bits = TXABitMasks[idxb];
//     else if (tx)
//         bits = TXABitMasks[idx];
//     else bits = RXABitMasks[idx];
// and mi0bot's HL2 branch keeps the same transmit rule:
//   From mi0bot-Thetis HPSDR/Penny.cs:176-181 [@c26a8a4]
// with idx / idxb the bands of VFOA / VFOB by VFO frequency:
//   From Thetis console.cs:29101-29102 [v2.10.3.15]
//     Band lo_band = BandByFreq(XVTRForm.TranslateFreq(VFOAFreq), ...);
//     Band lo_bandb = BandByFreq(XVTRForm.TranslateFreq(VFOBFreq), ...);
//
// Before this task the keyed byte read the RX1 stand-in's band whatever
// slice held the transmitter. On the HL2 these pins choose the N2ADR
// board's transmit low-pass, so a 40 m carrier on slice B with slice A on
// 20 m went out through the 30/20 m low-pass. And every band came from the
// DDC centre rather than the VFO, which differ under CTUN.
// =================================================================
//
// Modification history (NereusSDR):
//   2026-09-30 - HL2 unkeyed expects the higher slice's receive pins, not the
//                N2ADR bypass (0x00): JJ's ruling on HL2 Auto.
//                J.J. Boyd (KG4VCF), with AI-assisted implementation via
//                Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <algorithm>

#include "core/AppSettings.h"
#include "core/OcMatrix.h"
#include "core/P1RadioConnection.h"
#include "core/ReceiverManager.h"
#include "core/TxSliceArbiter.h"
#include "core/accessories/N2adrPreset.h"
#include "models/Band.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

constexpr double k20mHz = 14200000.0;
constexpr double k40mHz =  7100000.0;

class ConnectedP1 final : public P1RadioConnection {
public:
    ConnectedP1() { setState(ConnectionState::Connected); }
};

struct DetachConnection {
    RadioModel* model{nullptr};
    ~DetachConnection() { if (model) { model->injectConnectionForTest(nullptr); } }
};

// Bank 0 C2 = (ocByte << 1) & 0xFE.
quint8 ocByte(const P1RadioConnection& conn)
{
    quint8 bank0[5] = {};
    conn.composeCcForBankForTest(0, bank0);
    return quint8(bank0[2] >> 1);
}

// A model with an injected, Connected P1 connection and the production
// ReceiverManager -> connection wiring. The HL2 carries the N2ADR preset
// (its transmit masks: 40 m = 0x04, 20 m = 0x08); the Hermes carries a
// hand-made matrix with distinct receive and transmit pins per band.
struct Session {
    explicit Session(HPSDRHW board)
    {
        AppSettings::instance().clear();
        if (board == HPSDRHW::HermesLite) {
            applyN2adrPreset(oc, true);
        } else {
            oc.setPin(Band::Band20m, 0, /*tx=*/false, true);
            oc.setPin(Band::Band40m, 1, /*tx=*/false, true);
            oc.setPin(Band::Band40m, 2, /*tx=*/true,  true);
            oc.setPin(Band::Band20m, 3, /*tx=*/true,  true);
        }

        model.setBoardForTest(board);
        conn.setBoardForTest(board);
        conn.setOcMatrix(&oc);
        model.injectConnectionForTest(&conn);
        detach.model = &model;
        model.wireReceiverManagerHardwarePushesForTest();

        const int streams = (board == HPSDRHW::HermesLite) ? 2 : 4;
        model.configureStreamPool(streams, streams, 192000);
        for (int i = 0; i < streams; ++i) {
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

    OcMatrix         oc;
    RadioModel       model;
    ConnectedP1      conn;
    DetachConnection detach;
};

} // namespace

class TestP1BandOutputsFollowTxSlice : public QObject {
    Q_OBJECT

private slots:

    // ── Keyed: the transmitting slice's band, whichever slice that is ────
    void keyed_ocIsTheTransmittingSlicesMask_data()
    {
        QTest::addColumn<int>("board");
        QTest::addColumn<double>("aHz");
        QTest::addColumn<double>("bHz");
        QTest::addColumn<int>("txBand");
        QTest::newRow("HL2, A 20 m, B transmits 40 m")
            << int(HPSDRHW::HermesLite) << k20mHz << k40mHz << int(Band::Band40m);
        QTest::newRow("HL2, A 40 m, B transmits 20 m")
            << int(HPSDRHW::HermesLite) << k40mHz << k20mHz << int(Band::Band20m);
        QTest::newRow("Hermes, A 20 m, B transmits 40 m")
            << int(HPSDRHW::Hermes) << k20mHz << k40mHz << int(Band::Band40m);
        QTest::newRow("Hermes, A 40 m, B transmits 20 m")
            << int(HPSDRHW::Hermes) << k40mHz << k20mHz << int(Band::Band20m);
    }
    void keyed_ocIsTheTransmittingSlicesMask()
    {
        QFETCH(int, board);
        QFETCH(double, aHz);
        QFETCH(double, bHz);
        QFETCH(int, txBand);
        const HPSDRHW hw = HPSDRHW(board);
        Session s(hw);

        const int a = s.add(aHz);
        const int b = s.add(bHz);
        s.model.setActiveSlice(a);

        // Unkeyed first, to pin what the fix must leave alone. On the HL2 in
        // Auto the receive pins follow the highest-frequency slice (JJ's
        // ruling of 2026-09-30); elsewhere the receive mask of A, the RX1
        // stand-in.
        const quint8 unkeyed = (hw == HPSDRHW::HermesLite)
            ? s.oc.maskFor(bandFromFrequency(std::max(aHz, bHz)), /*tx=*/false)
            : s.oc.maskFor(bandFromFrequency(aHz), /*tx=*/false);
        QCOMPARE(ocByte(s.conn), unkeyed);

        QVERIFY(s.model.txSliceArbiter()->requestHandoff(b));
        QCOMPARE(s.model.txSliceArbiter()->txBoundSliceId(), b);
        s.conn.setMox(true);

        const quint8 expected = s.oc.maskFor(Band(txBand), /*tx=*/true);
        QVERIFY(expected != 0);
        QVERIFY(expected != s.oc.maskFor(bandFromFrequency(aHz), /*tx=*/true));
        QCOMPARE(ocByte(s.conn), expected);

        // The N2ADR transmit masks by name, from N2adrPreset.cpp: 40 m pin 3
        // (0x04), 20 m pin 4 (0x08).
        if (hw == HPSDRHW::HermesLite) {
            QCOMPARE(ocByte(s.conn),
                     Band(txBand) == Band::Band40m ? quint8(0x04) : quint8(0x08));
        }

        // Turning A's knob mid-transmission does not move the pins.
        s.model.sliceById(a)->setFrequency(aHz + 5000.0);
        QCOMPARE(ocByte(s.conn), expected);

        // Unkeyed again: back to the receive decision, unchanged.
        s.conn.setMox(false);
        QCOMPARE(ocByte(s.conn), unkeyed);

        // The transmitter handed back to A: keyed pins follow A.
        QVERIFY(s.model.txSliceArbiter()->requestHandoff(a));
        s.conn.setMox(true);
        QCOMPARE(ocByte(s.conn), s.oc.maskFor(bandFromFrequency(aHz), /*tx=*/true));
        s.conn.setMox(false);
    }

    // ── Keyed on B after A is closed (the bench script's first step) ─────
    void keyed_afterAIsClosed_followsB_data()
    {
        QTest::addColumn<int>("board");
        QTest::newRow("HL2")    << int(HPSDRHW::HermesLite);
        QTest::newRow("Hermes") << int(HPSDRHW::Hermes);
    }
    void keyed_afterAIsClosed_followsB()
    {
        QFETCH(int, board);
        Session s{HPSDRHW(board)};

        const int a = s.add(k20mHz);
        const int b = s.add(k40mHz);
        s.model.removeSlice(a);
        QVERIFY(s.model.txSliceArbiter()->requestHandoff(b));
        s.conn.setMox(true);
        QCOMPARE(ocByte(s.conn), s.oc.maskFor(Band::Band40m, /*tx=*/true));
        s.conn.setMox(false);
        QCOMPARE(ocByte(s.conn), s.oc.maskFor(Band::Band40m, /*tx=*/false));
    }

    // ── The band comes from the VFO, not the DDC centre ──────────────────
    //
    // With CTUN the DDC centre can sit outside the band the VFO is in. Here
    // slice A's VFO is inside 20 m while its stream is centred below 14 MHz.
    // Thetis's band is BandByFreq(VFOAFreq), the VFO.
    void unkeyed_bandIsTheVfosNotTheCentres_data()
    {
        keyed_afterAIsClosed_followsB_data();
    }
    void unkeyed_bandIsTheVfosNotTheCentres()
    {
        QFETCH(int, board);
        Session s{HPSDRHW(board)};

        constexpr double kVfoHz    = 14010000.0;   // 20 m
        constexpr double kCentreHz = 13950000.0;   // below the 20 m edge
        QCOMPARE(bandFromFrequency(kVfoHz), Band::Band20m);
        QVERIFY(bandFromFrequency(kCentreHz) != Band::Band20m);
        QVERIFY(s.oc.maskFor(bandFromFrequency(kCentreHz), /*tx=*/false)
                != s.oc.maskFor(Band::Band20m, /*tx=*/false));

        const int a = s.add(kVfoHz);
        QVERIFY(s.model.requestStreamCentre(a, kCentreHz));
        QCOMPARE(s.model.sliceById(a)->frequency(), kVfoHz);

        QCOMPARE(ocByte(s.conn), s.oc.maskFor(Band::Band20m, /*tx=*/false));
    }
};

QTEST_MAIN(TestP1BandOutputsFollowTxSlice)
#include "tst_p1_band_outputs_follow_tx_slice.moc"
