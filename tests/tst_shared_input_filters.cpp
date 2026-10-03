// no-port-check: test-only. Thetis and mi0bot file names appear only in
// source-cite comments that document which upstream line each assertion
// verifies. No upstream logic is ported here; this file is NereusSDR-original.
//
// =================================================================
// Shared-input filters: both filters on one receiver input follow the same
// counted slices, and the operator is told why the low-pass is held.
// =================================================================
//
// Ruling (c), 2026-09-30:
//   * The receive low-pass follows the highest-frequency slice counted on
//     the input, Thetis's "higher of the two" generalised to every slice:
//       From Thetis console.cs:15491-15495 UpdateAlexTXFilter [v2.10.3.15]
//         if (!_rx2_preamp_present && chkRX2.Checked)
//         {
//             if (rx1_dds_freq_mhz > rx2_dds_freq_mhz) setAlexLPF(rx1_dds_freq_mhz, false);
//             else setAlexLPF(rx2_dds_freq_mhz, false);
//         }
//     and on the HL2 the N2ADR board's receive pins follow the high band:
//       From mi0bot-Thetis HPSDR/Penny.cs:185-188 [@c26a8a4]
//         if (Console.getConsole().RX2Enabled && (idxb > idx))     // MI0BOT: Select the filter for the high band
//             bits = RXABitMasks[idxb];
//         else
//             bits = RXABitMasks[idx];
//   * The band-pass bypass rule is unchanged.
//   * Both use the same counted set, with one away rule (Amendment 8a).
// Ruling (d): the chain's state carries the low-pass reason and the slice
// that forces it (AlexAdcState::lowPassReason, lowPassSlice).
//
// Modification history (NereusSDR):
//   2026-09-30  J.J. Boyd / KG4VCF  Created (shared-input filters, rulings
//                                    (c) and (d)). AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-30  J.J. Boyd / KG4VCF  Follow-up: CTUN at a low-pass edge
//                                    (the reason names the filter's
//                                    slice); an HL2 pin edit refreshes
//                                    the held reason. AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-30  J.J. Boyd / KG4VCF  Review fix: the HL2 Auto bypass
//                                    clears the reason; P2 away rule;
//                                    6m/ByPass on RX empties the reason.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-30  J.J. Boyd / KG4VCF  HL2 pins by frequency (maintainer
//                                    ruling): 20 m + 10 m, 49 m SWL,
//                                    WWV and an equal-frequency tie.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-30  J.J. Boyd / KG4VCF  HL2 Auto keeps the pins of the
//                                    highest slice with the N2ADR preset
//                                    (JJ's ruling): bit 6 off for 160 m
//                                    and for a user table without it,
//                                    ForceBypass 0x00, keyed TX pins.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-30  J.J. Boyd / KG4VCF  Review fix: the top slice named when
//                                    its own mask lacks pin 7; the
//                                    high-pass-only reason; Force band
//                                    with 160 m; 6m/ByPass on the HL2;
//                                    the 6.0 MHz case renamed for GEN;
//                                    WWV over 20 m reports the board off.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-30  J.J. Boyd / KG4VCF  The pin edit's hold is announced on
//                                    lowPassHoldChanged, its own
//                                    notifier. AI-assisted via Anthropic
//                                    Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <memory>

#include "core/AppSettings.h"
#include "core/BoardCapabilities.h"
#include "core/OcMatrix.h"
#include "core/P1RadioConnection.h"
#include "core/P2RadioConnection.h"
#include "core/ReceiverManager.h"
#include "core/accessories/AlexController.h"
#include "core/codec/AlexFilterMap.h"
#include "core/codec/P2CodecSaturn.h"
#include "core/SliceOwnership.h"
#include "core/accessories/N2adrPreset.h"
#include "models/Band.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "OperatorWording.h"

using namespace NereusSDR;

namespace {

constexpr double k6mHz   = 50125000.0;
constexpr double k10mHz  = 28400000.0;
constexpr double k12mHz  = 24940000.0;
constexpr double k15mHz  = 21200000.0;
constexpr double k17mHz  = 18100000.0;
constexpr double k20mHz  = 14200000.0;
constexpr double k30mHz  = 10120000.0;
constexpr double k40mHz  =  7100000.0;
constexpr double k60mHz  =  5357000.0;
constexpr double k80mHz  =  3700000.0;
constexpr double k160mHz =  1850000.0;

constexpr int kAlex0Offset = 1432;

class ConnectedP1 final : public P1RadioConnection {
public:
    ConnectedP1() { setState(ConnectionState::Connected); }
};

class ConnectedP2 final : public P2RadioConnection {
public:
    ConnectedP2() { setState(ConnectionState::Connected); }
};

struct DetachConnection {
    RadioModel* model{nullptr};
    ~DetachConnection() { if (model) { model->injectConnectionForTest(nullptr); } }
};

// ── Protocol 1 wire reads ──────────────────────────────────────────────────
quint8 p1HpfBits(const P1RadioConnection& conn)
{
    return quint8(conn.captureBank10ForTest()[3]) & 0x7F;   // bit 7 is the T/R relay
}

quint8 p1LpfBits(const P1RadioConnection& conn)
{
    return quint8(conn.captureBank10ForTest()[4]);
}

quint8 p1OcByte(const P1RadioConnection& conn)
{
    quint8 bank0[5] = {};
    conn.composeCcForBankForTest(0, bank0);
    return quint8(bank0[2] >> 1);
}

// ── Protocol 2 wire reads ──────────────────────────────────────────────────
quint32 readBE32(const quint8* buf, int offset)
{
    return (quint32(buf[offset])     << 24)
         | (quint32(buf[offset + 1]) << 16)
         | (quint32(buf[offset + 2]) << 8)
         |  quint32(buf[offset + 3]);
}

// Inverse of the LPF scatter in P2CodecOrionMkII::buildAlex0.
// Bit map from Thetis ChannelMaster/netInterface.c:691-702 [v2.10.3.15].
quint8 lpfMaskFromReg(quint32 reg)
{
    quint8 bits = 0;
    if (reg & (1u << 20)) { bits |= 0x01; }
    if (reg & (1u << 21)) { bits |= 0x02; }
    if (reg & (1u << 22)) { bits |= 0x04; }
    if (reg & (1u << 23)) { bits |= 0x08; }
    if (reg & (1u << 29)) { bits |= 0x10; }
    if (reg & (1u << 30)) { bits |= 0x20; }
    if (reg & (1u << 31)) { bits |= 0x40; }
    return bits;
}

quint8 p2Alex0Lpf(P2RadioConnection& conn)
{
    quint8 buf[1444] = {};
    conn.composeCmdHighPriorityForTest(buf);
    return lpfMaskFromReg(readBE32(buf, kAlex0Offset));
}

quint32 p2Alex0Reg(P2RadioConnection& conn)
{
    quint8 buf[1444] = {};
    conn.composeCmdHighPriorityForTest(buf);
    return readBE32(buf, kAlex0Offset);
}

// A Protocol 1 radio with an injected, Connected connection and the
// production ReceiverManager -> connection wiring.
struct P1Session {
    explicit P1Session(HPSDRHW board)
    {
        AppSettings::instance().clear();
        oc.setPin(Band::Band20m, 0, /*tx=*/false, true);
        oc.setPin(Band::Band40m, 1, /*tx=*/false, true);
        oc.setPin(Band::Band80m, 2, /*tx=*/false, true);

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
    ~P1Session() { AppSettings::instance().clear(); }

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

// An ANAN-G2 (Saturn board, P2CodecSaturn) with an injected, Connected
// connection and the production ReceiverManager -> connection wiring.
struct G2Session {
    G2Session()
    {
        AppSettings::instance().clear();
        model.setBoardForTest(HPSDRHW::Saturn);
        conn.setBoardForTest(HPSDRHW::Saturn);
        model.configureStreamPool(5, 5, 192000);
        model.receiverManager()->setP2Codec(&codec);
        model.injectConnectionForTest(&conn);
        detach.model = &model;
        model.wireReceiverManagerHardwarePushesForTest();
        for (int st = 0; st < 5; ++st) {
            model.receiverManager()->createReceiver();
        }
    }
    ~G2Session() { AppSettings::instance().clear(); }

    int add(double hz)
    {
        const int id = model.addSlice();
        model.sliceById(id)->setFrequency(hz);
        return id;
    }

    P2CodecSaturn    codec;
    RadioModel       model;
    ConnectedP2      conn;
    DetachConnection detach;
};

// The model names the held slices from its own OcMatrix; point the
// connection at it too, so the pins on the wire and in the reason agree.
// P1Session's defaults (20 m pin 0, 40 m pin 1, 80 m pin 2) carried over.
OcMatrix& sharedPins(RadioModel& model, P1RadioConnection& conn)
{
    OcMatrix& oc = model.ocMatrixMutable();
    conn.setOcMatrix(&oc);
    oc.setPin(Band::Band20m, 0, /*tx=*/false, true);
    oc.setPin(Band::Band40m, 1, /*tx=*/false, true);
    oc.setPin(Band::Band80m, 2, /*tx=*/false, true);
    return oc;
}

// The same, with the N2ADR preset's pins: 160 m pin 1 alone, 80-10 m the
// band's pin plus pin 7 (bit 6, the broadcast-band high-pass) on receive.
OcMatrix& n2adrPins(RadioModel& model, P1RadioConnection& conn)
{
    OcMatrix& oc = model.ocMatrixMutable();
    conn.setOcMatrix(&oc);
    applyN2adrPreset(oc, true);
    return oc;
}

constexpr quint8 kHighPassBit = 0x40;   // N2ADR pin 7, bit 6

QString letterOn(const RadioModel& model, int id, const char* band)
{
    return QStringLiteral("%1 on %2").arg(model.sliceById(id)->sliceLetter(),
                                          QLatin1String(band));
}

} // namespace

class TestSharedInputFilters : public QObject {
    Q_OBJECT

private slots:

    // ── 80 m and 20 m on one input: the low-pass is the higher slice's ───
    //
    // Slice A (RX1) on 80 m, slice B on 20 m. Thetis's RX1 rule would leave
    // A's 80 m low-pass in front of B on the boards with an RX2 front end;
    // the counted rule passes B. The band-pass bypasses as it did, and the
    // chain names B as the slice holding the low-pass.
    void p1_twoBands_lowPassFollowsTheHigherSlice_data()
    {
        QTest::addColumn<int>("board");
        QTest::newRow("Hermes / ANAN-10 / ANAN-100 (one ADC)") << int(HPSDRHW::Hermes);
        QTest::newRow("ANAN-100D (two ADCs, one bank)")        << int(HPSDRHW::Angelia);
        QTest::newRow("ANAN-G2E (HermesC10)")                  << int(HPSDRHW::HermesC10);
    }
    void p1_twoBands_lowPassFollowsTheHigherSlice()
    {
        QFETCH(int, board);
        const HPSDRHW hw = HPSDRHW(board);
        P1Session s(hw);

        const int a = s.add(k80mHz);
        QCOMPARE(p1LpfBits(s.conn), codec::alex::computeLpf(k80mHz / 1e6));
        QCOMPARE(s.model.filterChainState(0).lowPassSlice, -1);

        const int b = s.add(k20mHz);
        QCOMPARE(s.model.sliceChainIndex(a), 0);
        QCOMPARE(s.model.sliceChainIndex(b), 0);

        // The low-pass: the higher slice's.
        QCOMPARE(p1LpfBits(s.conn), codec::alex::computeLpf(k20mHz / 1e6));
        // The band-pass: bypassed, as today.
        QCOMPARE(s.model.filterChainState(0).effective, AlexController::BpfEffective::Bypass);
        QCOMPARE(p1HpfBits(s.conn), quint8(0x20));
        // The reason names B, and A below it.
        const AlexController::AlexAdcState& st = s.model.filterChainState(0);
        QCOMPARE(st.lowPassSlice, b);
        QVERIFY2(st.lowPassReason.contains(letterOn(s.model, b, "20m")),
                 qPrintable(st.lowPassReason));
        QVERIFY2(st.lowPassReason.contains(letterOn(s.model, a, "80m")),
                 qPrintable(st.lowPassReason));
        QVERIFY2(OperatorWording::isPlain(st.lowPassReason), qPrintable(st.lowPassReason));
        QVERIFY(s.model.rxFilter0LowPassReason() == st.lowPassReason);
        QCOMPARE(s.model.rxFilter0LowPassSlice(), b);

        // B closed: A alone again, and nothing is held.
        s.model.removeSlice(b);
        QCOMPARE(p1LpfBits(s.conn), codec::alex::computeLpf(k80mHz / 1e6));
        QCOMPARE(s.model.filterChainState(0).lowPassSlice, -1);
        QVERIFY(s.model.filterChainState(0).lowPassReason.isEmpty());
    }

    void g2_twoBands_lowPassFollowsTheHigherSlice()
    {
        G2Session s;
        const int a = s.add(k80mHz);
        const int b = s.add(k20mHz);
        QCOMPARE(s.model.sliceChainIndex(a), 0);
        QCOMPARE(s.model.sliceChainIndex(b), 0);

        // Thetis gives the G2 RX1's low-pass (80 m); the counted rule passes
        // the higher slice on the shared input.
        QCOMPARE(p2Alex0Lpf(s.conn), codec::alex::computeLpf(k20mHz / 1e6));
        QCOMPARE(s.model.filterChainState(0).effective, AlexController::BpfEffective::Bypass);
        QCOMPARE(s.model.filterChainState(0).lowPassSlice, b);
        QVERIFY2(s.model.filterChainState(0).lowPassReason.contains(letterOn(s.model, b, "20m")),
                 qPrintable(s.model.filterChainState(0).lowPassReason));

        // B retuned below A: A's low-pass, and nothing below A is held.
        s.model.sliceById(b)->setFrequency(k160mHz);
        QCOMPARE(p2Alex0Lpf(s.conn), codec::alex::computeLpf(k80mHz / 1e6));
        QCOMPARE(s.model.filterChainState(0).lowPassSlice, a);
    }

    // ── One slice: the selection is what it was, on every band ──────────
    void oneSlice_everyBandUnchanged_data()
    {
        QTest::addColumn<int>("board");
        QTest::newRow("Hermes") << int(HPSDRHW::Hermes);
        QTest::newRow("ANAN-100D") << int(HPSDRHW::Angelia);
        QTest::newRow("HL2") << int(HPSDRHW::HermesLite);
    }
    void oneSlice_everyBandUnchanged()
    {
        QFETCH(int, board);
        const HPSDRHW hw = HPSDRHW(board);
        P1Session s(hw);
        const int a = s.add(k160mHz);
        for (double hz : {k160mHz, k80mHz, k60mHz, k40mHz, k30mHz, k20mHz, k17mHz,
                          k15mHz, k12mHz, k10mHz, k6mHz}) {
            s.model.sliceById(a)->setFrequency(hz);
            QCOMPARE(p1LpfBits(s.conn), codec::alex::computeLpf(hz / 1e6));
            QCOMPARE(p1OcByte(s.conn), s.oc.maskFor(bandFromFrequency(hz), /*tx=*/false));
            QCOMPARE(s.model.filterChainState(0).lowPassSlice, -1);
            QVERIFY(s.model.filterChainState(0).lowPassReason.isEmpty());
        }
    }

    void g2_oneSlice_everyBandUnchanged()
    {
        G2Session s;
        const int a = s.add(k160mHz);
        for (double hz : {k160mHz, k80mHz, k60mHz, k40mHz, k30mHz, k20mHz, k17mHz,
                          k15mHz, k12mHz, k10mHz, k6mHz}) {
            s.model.sliceById(a)->setFrequency(hz);
            QCOMPARE(p2Alex0Lpf(s.conn), codec::alex::computeLpf(hz / 1e6));
            QCOMPARE(s.model.filterChainState(0).lowPassSlice, -1);
        }
    }

    // ── A slice the away rule leaves out changes neither filter ─────────
    //
    // Slice A on 80 m (device B's), slice B on 20 m (device A's). With
    // device A away, B no longer counts: the band-pass filters for A alone,
    // as it already did (Amendment 8a), and now so does the low-pass.
    void anAwaySlice_changesNeitherFilter()
    {
        P1Session s(HPSDRHW::Hermes);
        const int a = s.add(k80mHz);
        const int b = s.add(k20mHz);
        SliceOwnership* ownership = s.model.sliceOwnership();
        QVERIFY(ownership != nullptr);
        const QByteArray devA = QByteArrayLiteral("device-a");
        ownership->setOwner(a, QByteArrayLiteral("device-b"));
        ownership->setOwner(b, devA);
        s.model.requestDdcAssignment();
        QCOMPARE(p1LpfBits(s.conn), codec::alex::computeLpf(k20mHz / 1e6));
        QCOMPARE(p1HpfBits(s.conn), quint8(0x20));
        QCOMPARE(s.model.filterChainState(0).lowPassSlice, b);

        ownership->setAwayDevices({devA});
        QVERIFY(ownership->isAwaySlice(b));
        QCOMPARE(p1LpfBits(s.conn), codec::alex::computeLpf(k80mHz / 1e6));
        QCOMPARE(p1HpfBits(s.conn),
                 codec::alex::computeRxPreselector(k80mHz / 1e6, HPSDRHW::Hermes));
        QCOMPARE(s.model.filterChainState(0).effective, AlexController::BpfEffective::Filtered);
        QCOMPARE(s.model.filterChainState(0).lowPassSlice, -1);
        QVERIFY(s.model.filterChainState(0).lowPassReason.isEmpty());

        // Retuning the away slice moves neither filter.
        s.model.sliceById(b)->setFrequency(k10mHz);
        QCOMPARE(p1LpfBits(s.conn), codec::alex::computeLpf(k80mHz / 1e6));
        QCOMPARE(p1HpfBits(s.conn),
                 codec::alex::computeRxPreselector(k80mHz / 1e6, HPSDRHW::Hermes));

        // Back: B counts again at once.
        ownership->setAwayDevices({});
        QCOMPARE(p1LpfBits(s.conn), codec::alex::computeLpf(k10mHz / 1e6));
        QCOMPARE(p1HpfBits(s.conn), quint8(0x20));
        QCOMPARE(s.model.filterChainState(0).lowPassSlice, b);
    }

    // The same on Protocol 2 (ANAN-G2): the away slice leaves the Alex0
    // register, band-pass and low-pass together, where slice A puts it.
    void g2_anAwaySlice_changesNeitherFilter()
    {
        G2Session s;
        const int a = s.add(k80mHz);
        const int b = s.add(k20mHz);
        SliceOwnership* ownership = s.model.sliceOwnership();
        QVERIFY(ownership != nullptr);
        const QByteArray devA = QByteArrayLiteral("device-a");
        ownership->setOwner(a, QByteArrayLiteral("device-b"));
        ownership->setOwner(b, devA);
        s.model.requestDdcAssignment();
        QCOMPARE(p2Alex0Lpf(s.conn), codec::alex::computeLpf(k20mHz / 1e6));
        QCOMPARE(s.model.filterChainState(0).effective, AlexController::BpfEffective::Bypass);
        QCOMPARE(s.model.filterChainState(0).lowPassSlice, b);

        ownership->setAwayDevices({devA});
        QVERIFY(ownership->isAwaySlice(b));
        QCOMPARE(p2Alex0Lpf(s.conn), codec::alex::computeLpf(k80mHz / 1e6));
        QCOMPARE(s.model.filterChainState(0).effective, AlexController::BpfEffective::Filtered);
        QCOMPARE(s.model.filterChainState(0).lowPassSlice, -1);
        QVERIFY(s.model.filterChainState(0).lowPassReason.isEmpty());
        const quint32 aloneReg = p2Alex0Reg(s.conn);

        // Retuning the away slice moves neither filter.
        s.model.sliceById(b)->setFrequency(k10mHz);
        QCOMPARE(p2Alex0Reg(s.conn), aloneReg);
        QCOMPARE(s.model.filterChainState(0).lowPassSlice, -1);

        // Back: B counts again at once.
        ownership->setAwayDevices({});
        QCOMPARE(p2Alex0Lpf(s.conn), codec::alex::computeLpf(k10mHz / 1e6));
        QCOMPARE(s.model.filterChainState(0).effective, AlexController::BpfEffective::Bypass);
        QCOMPARE(s.model.filterChainState(0).lowPassSlice, b);
    }

    // ── 6m/ByPass on RX: one low-pass for every slice, nothing held ──────
    //
    // With the switch on, the receive low-pass is the 6 m filter whatever
    // the slices are tuned to (codec::alex::setAlexLpf), so no slice is
    // held behind another's and the reason is empty.
    void lpfBypassOnRx_leavesTheReasonEmpty()
    {
        const QString mac = QStringLiteral("AA:BB:CC:DD:EE:5F");
        P1Session s(HPSDRHW::Hermes);
        RadioInfo info;
        info.macAddress = mac;
        info.boardType = HPSDRHW::Hermes;
        s.model.setLastRadioInfoForTest(info);
        s.model.setConnectionStateForTest(ConnectionState::Connected);

        const int a = s.add(k80mHz);
        const int b = s.add(k20mHz);
        QCOMPARE(s.model.sliceChainIndex(a), 0);
        QCOMPARE(s.model.filterChainState(0).lowPassSlice, b);
        QVERIFY(!s.model.filterChainState(0).lowPassReason.isEmpty());

        AppSettings::instance().setHardwareValue(mac, QStringLiteral("alex/master/lpfBypass"),
                                                 QStringLiteral("True"));
        s.model.applyAlexHpfSwitchSettings();
        QTRY_COMPARE(p1LpfBits(s.conn), codec::alex::computeLpf(k6mHz / 1e6));
        QCOMPARE(s.model.filterChainState(0).lowPassSlice, -1);
        QVERIFY(s.model.filterChainState(0).lowPassReason.isEmpty());
        QVERIFY(s.model.rxFilter0LowPassReason().isEmpty());

        // Off again: B holds A once more.
        AppSettings::instance().setHardwareValue(mac, QStringLiteral("alex/master/lpfBypass"),
                                                 QStringLiteral("False"));
        s.model.applyAlexHpfSwitchSettings();
        QTRY_COMPARE(p1LpfBits(s.conn), codec::alex::computeLpf(k20mHz / 1e6));
        QCOMPARE(s.model.filterChainState(0).lowPassSlice, b);
    }

    // ── The HL2 pins follow the highest-frequency slice ─────────────────
    //
    // The N2ADR board's receive pins are the HL2's filter, a bank of
    // low-passes. With the policy forcing a filter (so the multi-band bypass
    // does not clear the pins) the pins are the band's of the slice with the
    // highest frequency, whichever slice is RX1 (maintainer ruling
    // 2026-09-30; mi0bot's own compare is by band enum).
    void hl2_receivePinsFollowTheHighBand()
    {
        P1Session s(HPSDRHW::HermesLite);
        s.oc.setPin(Band::Band10m, 3, /*tx=*/false, true);
        s.model.alexControllerMutable().setBpfMode(0, AlexController::BpfMode::ForceBand);

        const int a = s.add(k80mHz);
        const int b = s.add(k20mHz);
        QCOMPARE(s.conn.rx1SlotForTest(), 0);
        QCOMPARE(p1OcByte(s.conn), s.oc.maskFor(Band::Band20m, /*tx=*/false));
        QCOMPARE(s.model.filterChainState(0).lowPassSlice, b);

        // RX1 above B: RX1's band.
        s.model.sliceById(a)->setFrequency(k10mHz);
        QCOMPARE(p1OcByte(s.conn), s.oc.maskFor(Band::Band10m, /*tx=*/false));
        QCOMPARE(s.model.filterChainState(0).lowPassSlice, a);

        // Same band on both: one set of pins, and nothing is held.
        s.model.sliceById(b)->setFrequency(k10mHz + 10000.0);
        QCOMPARE(p1OcByte(s.conn), s.oc.maskFor(Band::Band10m, /*tx=*/false));
        QCOMPARE(s.model.filterChainState(0).lowPassSlice, -1);
    }

    // 20 m and 10 m: the pins follow 10 m, the higher frequency.
    void hl2_twentyAndTen_pinsFollowTen()
    {
        P1Session s(HPSDRHW::HermesLite);
        OcMatrix& oc = sharedPins(s.model, s.conn);
        oc.setPin(Band::Band10m, 3, /*tx=*/false, true);
        s.model.alexControllerMutable().setBpfMode(0, AlexController::BpfMode::ForceBand);
        const int a = s.add(k20mHz);
        const int b = s.add(k10mHz);
        QCOMPARE(p1OcByte(s.conn), oc.maskFor(Band::Band10m, /*tx=*/false));
        const AlexController::AlexAdcState& st = s.model.filterChainState(0);
        QCOMPARE(st.lowPassSlice, b);
        QVERIFY2(st.lowPassReason.contains(letterOn(s.model, b, "10m")), qPrintable(st.lowPassReason));
        QVERIFY2(st.lowPassReason.contains(letterOn(s.model, a, "20m")), qPrintable(st.lowPassReason));
    }

    // RX1 at 6.0 MHz, slice B on 20 m. NereusSDR's band lookup has no SWL
    // ranges, so 6.0 MHz is GEN, a band outside mi0bot's range
    // (OcMatrix::extCtrlBandIndex -1, where mi0bot sends no pins,
    // Penny.cs:162-165 [@c26a8a4]). It ranks below every band in range, so
    // the pins follow 20 m and the reason names the 20 m slice. (SWL enum
    // order cannot be reached by frequency here; WWV, below, is the case
    // that tests the frequency ordering against mi0bot's enum.)
    void hl2_genAndTwenty_pinsFollowTwenty()
    {
        constexpr double kGenHz = 6000000.0;
        P1Session s(HPSDRHW::HermesLite);
        OcMatrix& oc = sharedPins(s.model, s.conn);
        s.model.alexControllerMutable().setBpfMode(0, AlexController::BpfMode::ForceBand);
        QCOMPARE(bandFromFrequency(kGenHz), Band::GEN);
        const int a = s.add(kGenHz);
        const int b = s.add(k20mHz);
        QCOMPARE(s.conn.rx1SlotForTest(), 0);
        QCOMPARE(p1OcByte(s.conn), oc.maskFor(Band::Band20m, /*tx=*/false));
        const AlexController::AlexAdcState& st = s.model.filterChainState(0);
        QCOMPARE(st.lowPassSlice, b);
        QVERIFY2(st.lowPassReason.startsWith(
                     QStringLiteral("The receive low-pass filter is set for slice %1,")
                         .arg(letterOn(s.model, b, "20m"))),
                 qPrintable(st.lowPassReason));
        QVERIFY2(st.lowPassReason.contains(QStringLiteral("%1 on ")
                                               .arg(s.model.sliceById(a)->sliceLetter())),
                 qPrintable(st.lowPassReason));
    }

    // RX1 on 20 m, slice B on WWV at 10 MHz. mi0bot's enum puts WWV
    // (idx 12) above B20M (idx 5) and would send the WWV pins; by frequency
    // the pins follow 20 m and the reason names RX1's 20 m slice.
    void hl2_wwvAndTwenty_pinsFollowTwenty()
    {
        constexpr double kWwv10Hz = 10000000.0;
        P1Session s(HPSDRHW::HermesLite);
        OcMatrix& oc = sharedPins(s.model, s.conn);
        oc.setPin(Band::WWV, 4, /*tx=*/false, true);
        s.model.alexControllerMutable().setBpfMode(0, AlexController::BpfMode::ForceBand);
        const int a = s.add(k20mHz);
        const int b = s.add(kWwv10Hz);
        QCOMPARE(bandFromFrequency(kWwv10Hz), Band::WWV);
        QCOMPARE(p1OcByte(s.conn), oc.maskFor(Band::Band20m, /*tx=*/false));
        const AlexController::AlexAdcState& st = s.model.filterChainState(0);
        QCOMPARE(st.lowPassSlice, a);
        QVERIFY2(st.lowPassReason.contains(letterOn(s.model, a, "20m")), qPrintable(st.lowPassReason));
        QVERIFY2(st.lowPassReason.contains(letterOn(s.model, b, "WWV")), qPrintable(st.lowPassReason));
    }

    // An equal-frequency tie goes to RX1. Under CTUN both receivers sit on
    // one centre, 10.05 MHz: RX1's VFO on WWV (10.0 MHz), B's on 30 m
    // (10.12 MHz). The pins are RX1's band's (WWV), and B is held.
    void hl2_equalFrequencyTie_rx1Keeps()
    {
        constexpr double kCentre = 10050000.0;
        constexpr double kVfoA = 10000000.0;     // WWV
        constexpr double kFirstB = 10300000.0;   // outside A's window: B gets its own DDC
        constexpr double kVfoB = 10120000.0;     // 30 m
        constexpr double kStepCentre = 10220000.0;
        constexpr double kStepVfo = 10130000.0;
        P1Session s(HPSDRHW::HermesLite);
        OcMatrix& oc = sharedPins(s.model, s.conn);
        oc.setPin(Band::WWV, 4, /*tx=*/false, true);
        oc.setPin(Band::Band30m, 5, /*tx=*/false, true);
        s.model.alexControllerMutable().setBpfMode(0, AlexController::BpfMode::ForceBand);

        const int a = s.add(kVfoA);
        QVERIFY(s.model.requestStreamCtunPinned(a, true));
        QVERIFY(s.model.requestStreamCentre(a, kCentre));
        const int b = s.add(kFirstB);
        QVERIFY(s.model.requestStreamCtunPinned(b, true));
        // Walk B's centre down to A's, keeping B's VFO inside its window.
        QVERIFY(s.model.requestStreamCentre(b, kStepCentre));
        s.model.sliceById(b)->setFrequency(kStepVfo);
        QVERIFY(s.model.requestStreamCentre(b, kCentre));
        s.model.sliceById(b)->setFrequency(kVfoB);

        const int streamA = s.model.sliceById(a)->streamIndex();
        const int streamB = s.model.sliceById(b)->streamIndex();
        QVERIFY(streamA >= 0 && streamB >= 0 && streamA != streamB);
        QCOMPARE(s.model.streamCentreHzForTest(streamA), kCentre);
        QCOMPARE(s.model.streamCentreHzForTest(streamB), kCentre);
        QCOMPARE(s.conn.rx1SlotForTest(), 0);

        QCOMPARE(p1OcByte(s.conn), oc.maskFor(Band::WWV, /*tx=*/false));
        const AlexController::AlexAdcState& st = s.model.filterChainState(0);
        QCOMPARE(st.lowPassSlice, a);
        QVERIFY2(st.lowPassReason.contains(letterOn(s.model, a, "WWV")), qPrintable(st.lowPassReason));
        QVERIFY2(st.lowPassReason.contains(letterOn(s.model, b, "30m")), qPrintable(st.lowPassReason));
    }

    // ── HL2 in Auto: the pins of the highest slice, not 0x00 ─────────────
    //
    // JJ's ruling of 2026-09-30: differing N2ADR masks in Auto no longer
    // switch the board off; the pins are the receive mask of the
    // highest-frequency counted slice, mi0bot's way, and the chain reports
    // Filtered (no WIDE). Bit 6, the board's broadcast-band high-pass, stays
    // on only when every counted slice's own mask has it. The held reason
    // names the slice the pins follow. Round 1 had the Auto case expect 0x00
    // and no reason; the 0x00 was the Auto bypass this ruling removes. With
    // the N2ADR preset.
    void hl2Auto_twoMasks_pinsFollowTheHighestSlice_data()
    {
        QTest::addColumn<double>("lowHz");
        QTest::addColumn<double>("highHz");
        QTest::addColumn<int>("highBand");
        QTest::addColumn<QString>("highName");
        QTest::addColumn<QString>("lowName");
        QTest::newRow("80 m + 40 m") << k80mHz << k40mHz << int(Band::Band40m)
                                     << QStringLiteral("40m") << QStringLiteral("80m");
        QTest::newRow("80 m + 20 m") << k80mHz << k20mHz << int(Band::Band20m)
                                     << QStringLiteral("20m") << QStringLiteral("80m");
        QTest::newRow("40 m + 20 m") << k40mHz << k20mHz << int(Band::Band20m)
                                     << QStringLiteral("20m") << QStringLiteral("40m");
    }
    void hl2Auto_twoMasks_pinsFollowTheHighestSlice()
    {
        QFETCH(double, lowHz);
        QFETCH(double, highHz);
        QFETCH(int, highBand);
        QFETCH(QString, highName);
        QFETCH(QString, lowName);
        P1Session s(HPSDRHW::HermesLite);
        OcMatrix& oc = n2adrPins(s.model, s.conn);
        const int a = s.add(lowHz);
        const int b = s.add(highHz);
        QVERIFY(oc.maskFor(bandFromFrequency(lowHz), false)
                != oc.maskFor(bandFromFrequency(highHz), false));
        QCOMPARE(s.conn.rx1SlotForTest(), 0);

        const quint8 wire = p1OcByte(s.conn);
        QCOMPARE(wire, oc.maskFor(Band(highBand), /*tx=*/false));
        QVERIFY(wire & kHighPassBit);
        const AlexController::AlexAdcState& st = s.model.filterChainState(0);
        QCOMPARE(st.mode, AlexController::BpfMode::Auto);
        QCOMPARE(st.effective, AlexController::BpfEffective::Filtered);
        QVERIFY(!s.model.panBypassState({a, b}).bypassed);
        QCOMPARE(st.lowPassSlice, b);
        QVERIFY2(st.lowPassReason.startsWith(
                     QStringLiteral("The receive low-pass filter is set for slice %1,")
                         .arg(letterOn(s.model, b, qPrintable(highName)))),
                 qPrintable(st.lowPassReason));
        QVERIFY2(st.lowPassReason.contains(letterOn(s.model, a, qPrintable(lowName))),
                 qPrintable(st.lowPassReason));
        QVERIFY2(!st.lowPassReason.contains(QStringLiteral("high-pass")),
                 qPrintable(st.lowPassReason));
    }

    // 160 m + 40 m: the preset gives 160 m no bit 6, so the 40 m low-pass
    // goes out with the high-pass off. No 0x00, no WIDE, and the reason
    // names both slices: the one the low-pass follows, and the one that
    // needs the high-pass off. Before this ruling a 160 m slice kept the
    // Auto bypass (0x00).
    void hl2Auto_160mAndForty_fortyLowPassHighPassOff()
    {
        P1Session s(HPSDRHW::HermesLite);
        OcMatrix& oc = n2adrPins(s.model, s.conn);
        const int a = s.add(k160mHz);
        const int b = s.add(k40mHz);
        const quint8 forty = oc.maskFor(Band::Band40m, /*tx=*/false);
        QVERIFY(forty & kHighPassBit);
        QCOMPARE(p1OcByte(s.conn), quint8(forty & ~kHighPassBit));
        const AlexController::AlexAdcState& st = s.model.filterChainState(0);
        QCOMPARE(st.effective, AlexController::BpfEffective::Filtered);
        QVERIFY(!s.model.panBypassState({a, b}).bypassed);
        QCOMPARE(st.lowPassSlice, b);
        QCOMPARE(st.lowPassReason,
                 QStringLiteral("The receive low-pass filter is set for slice %1, the highest "
                                "band on this receiver input. Slice %2 shares the input, so it "
                                "has less protection from strong signals on higher bands. The "
                                "broadcast-band high-pass filter is off because slice %2 needs "
                                "it off.")
                     .arg(letterOn(s.model, b, "40m"), letterOn(s.model, a, "160m")));
    }

    // Two slices on 160 m share one mask: the 160 m pins, nothing held.
    void hl2Auto_160mAnd160m_the160mMask()
    {
        P1Session s(HPSDRHW::HermesLite);
        OcMatrix& oc = n2adrPins(s.model, s.conn);
        const int a = s.add(k160mHz);
        const int b = s.add(1950000.0);
        QCOMPARE(bandFromFrequency(1950000.0), Band::Band160m);
        QCOMPARE(p1OcByte(s.conn), oc.maskFor(Band::Band160m, /*tx=*/false));
        QCOMPARE(s.model.filterChainState(0).effective, AlexController::BpfEffective::Filtered);
        QVERIFY(!s.model.panBypassState({a, b}).bypassed);
        QCOMPARE(s.model.filterChainState(0).lowPassSlice, -1);
        QVERIFY(s.model.filterChainState(0).lowPassReason.isEmpty());
    }

    // A user pin table where 20 m has no pin 7: 40 m + 20 m sends 20 m's
    // pins, bit 6 clear (20 m's own mask has none to clear), and the reason
    // names the 20 m slice as the one the high-pass is off for. Then 20 m
    // under a 10 m slice that keeps pin 7: the 10 m pins go out with bit 6
    // cleared, and the reason says the 20 m slice needs it off.
    void hl2Auto_userTableTwentyWithoutPin7_clearsTheHighPass()
    {
        P1Session s(HPSDRHW::HermesLite);
        OcMatrix& oc = n2adrPins(s.model, s.conn);
        oc.setPin(Band::Band20m, 6, /*tx=*/false, false);
        oc.setPin(Band::Band10m, 6, /*tx=*/false, true);
        const int a = s.add(k40mHz);
        const int b = s.add(k20mHz);
        QCOMPARE(p1OcByte(s.conn), oc.maskFor(Band::Band20m, /*tx=*/false));
        QVERIFY(!(p1OcByte(s.conn) & kHighPassBit));
        QCOMPARE(s.model.filterChainState(0).lowPassSlice, b);
        // The high-pass is off for the 20 m slice itself, the one the pins
        // follow, so it is the slice named.
        QVERIFY2(s.model.filterChainState(0).lowPassReason.endsWith(
                     QStringLiteral("The broadcast-band high-pass filter is off because slice "
                                    "%1 needs it off.").arg(letterOn(s.model, b, "20m"))),
                 qPrintable(s.model.filterChainState(0).lowPassReason));

        // With the highest slice keeping pin 7 and a lower one without it,
        // the bit is cleared from the higher mask: 20 m + 10 m.
        s.model.sliceById(a)->setFrequency(k10mHz);
        const quint8 ten = oc.maskFor(Band::Band10m, /*tx=*/false);
        QVERIFY(ten & kHighPassBit);
        QCOMPARE(p1OcByte(s.conn), quint8(ten & ~kHighPassBit));
        const AlexController::AlexAdcState& st = s.model.filterChainState(0);
        QCOMPARE(st.effective, AlexController::BpfEffective::Filtered);
        QCOMPARE(st.lowPassSlice, a);
        QVERIFY2(st.lowPassReason.endsWith(
                     QStringLiteral("The broadcast-band high-pass filter is off because slice "
                                    "%1 needs it off.").arg(letterOn(s.model, b, "20m"))),
                 qPrintable(st.lowPassReason));
    }

    // Masks that differ only in pin 7: a user table with 30 m lacking it,
    // under a 20 m slice. One low-pass (the 30/20 m filter), so nothing is
    // held and lowPassSlice is -1; the reason is the high-pass sentence
    // alone, naming the 30 m slice.
    void hl2Auto_highPassOnly_reasonIsTheHighPassSentence()
    {
        P1Session s(HPSDRHW::HermesLite);
        OcMatrix& oc = n2adrPins(s.model, s.conn);
        oc.setPin(Band::Band30m, 6, /*tx=*/false, false);
        const int a = s.add(k30mHz);
        s.add(k20mHz);
        const quint8 twenty = oc.maskFor(Band::Band20m, /*tx=*/false);
        QCOMPARE(quint8(twenty & ~kHighPassBit), oc.maskFor(Band::Band30m, /*tx=*/false));
        QCOMPARE(p1OcByte(s.conn), quint8(twenty & ~kHighPassBit));
        const AlexController::AlexAdcState& st = s.model.filterChainState(0);
        QCOMPARE(st.effective, AlexController::BpfEffective::Filtered);
        QCOMPARE(st.lowPassSlice, -1);
        QCOMPARE(st.lowPassReason,
                 QStringLiteral("The broadcast-band high-pass filter is off because slice %1 "
                                "needs it off.").arg(letterOn(s.model, a, "30m")));
    }

    // Force band with a slice on 160 m: the pins are the highest slice's,
    // as in Auto (the HL2 has no forced-band pins), so bit 6 goes off and
    // the reason says so.
    void hl2_forceBand_160mAndForty_highPassOff()
    {
        P1Session s(HPSDRHW::HermesLite);
        OcMatrix& oc = n2adrPins(s.model, s.conn);
        s.model.alexControllerMutable().setBpfMode(0, AlexController::BpfMode::ForceBand);
        const int a = s.add(k160mHz);
        const int b = s.add(k40mHz);
        QCOMPARE(p1OcByte(s.conn),
                 quint8(oc.maskFor(Band::Band40m, /*tx=*/false) & ~kHighPassBit));
        const AlexController::AlexAdcState& st = s.model.filterChainState(0);
        QCOMPARE(st.effective, AlexController::BpfEffective::Filtered);
        QCOMPARE(st.lowPassSlice, b);
        QVERIFY2(st.lowPassReason.endsWith(
                     QStringLiteral("The broadcast-band high-pass filter is off because slice "
                                    "%1 needs it off.").arg(letterOn(s.model, a, "160m"))),
                 qPrintable(st.lowPassReason));
    }

    // 6m/ByPass on RX is an Alex switch; the HL2's pins do not read it, and
    // neither does its reason, so the two still agree.
    void hl2_lpfBypassOnRx_changesNeitherPinsNorReason()
    {
        const QString mac = QStringLiteral("AA:BB:CC:DD:EE:6F");
        P1Session s(HPSDRHW::HermesLite);
        RadioInfo info;
        info.macAddress = mac;
        info.boardType = HPSDRHW::HermesLite;
        s.model.setLastRadioInfoForTest(info);
        s.model.setConnectionStateForTest(ConnectionState::Connected);
        OcMatrix& oc = n2adrPins(s.model, s.conn);
        s.add(k160mHz);
        const int b = s.add(k40mHz);
        const quint8 wire = quint8(oc.maskFor(Band::Band40m, false) & ~kHighPassBit);
        QCOMPARE(p1OcByte(s.conn), wire);
        const QString reason = s.model.filterChainState(0).lowPassReason;
        QVERIFY(reason.contains(QStringLiteral("high-pass")));

        AppSettings::instance().setHardwareValue(mac, QStringLiteral("alex/master/lpfBypass"),
                                                 QStringLiteral("True"));
        s.model.applyAlexHpfSwitchSettings();
        QCOMPARE(p1OcByte(s.conn), wire);
        QCOMPARE(s.model.filterChainState(0).lowPassReason, reason);
        QCOMPARE(s.model.filterChainState(0).lowPassSlice, b);
        AppSettings::instance().setHardwareValue(mac, QStringLiteral("alex/master/lpfBypass"),
                                                 QStringLiteral("False"));
        s.model.applyAlexHpfSwitchSettings();
    }

    // ── HL2: the pins sent come to 0x00, the board is off ────────────────
    //
    // JJ's ruling of 2026-09-30 (option 2): the pins are the top slice's
    // receive mask as the rules say; when that comes to 0x00 because its
    // band has no pins set (WWV under the N2ADR preset), the board is
    // reported off: bypassed, WIDE naming that slice, no low-pass sentence,
    // lowPassSlice -1. The stock HL2 does not cover 6 m or 2 m, so WWV is
    // the band here.
    void hl2Auto_wwvOverTwenty_boardOff_data()
    {
        QTest::addColumn<double>("wwvHz");
        QTest::newRow("WWV 25 MHz") << 25000000.0;
        QTest::newRow("WWV 15 MHz") << 15000000.0;
    }
    void hl2Auto_wwvOverTwenty_boardOff()
    {
        QFETCH(double, wwvHz);
        P1Session s(HPSDRHW::HermesLite);
        OcMatrix& oc = n2adrPins(s.model, s.conn);
        QCOMPARE(bandFromFrequency(wwvHz), Band::WWV);
        QCOMPARE(oc.maskFor(Band::WWV, /*tx=*/false), quint8(0));
        const int a = s.add(k20mHz);
        const int b = s.add(wwvHz);
        QCOMPARE(p1OcByte(s.conn), quint8(0x00));
        const AlexController::AlexAdcState& st = s.model.filterChainState(0);
        QCOMPARE(st.effective, AlexController::BpfEffective::Bypass);
        QCOMPARE(st.lowPassSlice, -1);
        QVERIFY2(st.lowPassReason.isEmpty(), qPrintable(st.lowPassReason));
        const RadioModel::PanBypassState pan = s.model.panBypassState({a, b});
        QVERIFY(pan.bypassed);
        QCOMPARE(pan.reason,
                 QStringLiteral("Slice %1 has no filter pins set, so the filter board is off.")
                     .arg(letterOn(s.model, b, "WWV")));
        QVERIFY2(st.reasonText.contains(QStringLiteral("slice %1")
                                            .arg(letterOn(s.model, b, "WWV"))),
                 qPrintable(st.reasonText));

        // Retuned onto 17 m, the pins are set again: filtered, no WIDE.
        s.model.sliceById(b)->setFrequency(k17mHz);
        QCOMPARE(p1OcByte(s.conn), oc.maskFor(Band::Band17m, /*tx=*/false));
        QCOMPARE(s.model.filterChainState(0).effective, AlexController::BpfEffective::Filtered);
        QVERIFY(!s.model.panBypassState({a, b}).bypassed);
    }

    // A user table with 20 m's pins set and 40 m left empty: 40 m + 20 m
    // sends 20 m's mask. The top band has pins, so the board is not off.
    void hl2Auto_userTableFortyEmpty_twentyPins()
    {
        P1Session s(HPSDRHW::HermesLite);
        OcMatrix& oc = n2adrPins(s.model, s.conn);
        oc.setPin(Band::Band40m, 2, /*tx=*/false, false);
        oc.setPin(Band::Band40m, 6, /*tx=*/false, false);
        QCOMPARE(oc.maskFor(Band::Band40m, /*tx=*/false), quint8(0));
        const int a = s.add(k40mHz);
        const int b = s.add(k20mHz);
        const quint8 twenty = oc.maskFor(Band::Band20m, /*tx=*/false);
        QVERIFY(twenty != 0);
        // 40 m's empty mask lacks pin 7, so the high-pass goes off for it.
        QCOMPARE(p1OcByte(s.conn), quint8(twenty & ~kHighPassBit));
        const AlexController::AlexAdcState& st = s.model.filterChainState(0);
        QCOMPARE(st.effective, AlexController::BpfEffective::Filtered);
        QVERIFY(!s.model.panBypassState({a, b}).bypassed);
        QCOMPARE(st.lowPassSlice, b);
    }

    // ForceBypass still switches the board off: 0x00, WIDE, no reason.
    void hl2_forceBypass_sendsZero()
    {
        P1Session s(HPSDRHW::HermesLite);
        n2adrPins(s.model, s.conn);
        s.model.alexControllerMutable().setBpfMode(0, AlexController::BpfMode::ForceBypass);
        const int a = s.add(k160mHz);
        const int b = s.add(k40mHz);
        QCOMPARE(p1OcByte(s.conn), quint8(0x00));
        const AlexController::AlexAdcState& st = s.model.filterChainState(0);
        QCOMPARE(st.effective, AlexController::BpfEffective::Bypass);
        QVERIFY(s.model.panBypassState({a, b}).bypassed);
        QCOMPARE(st.lowPassSlice, -1);
        QVERIFY(st.lowPassReason.isEmpty());
    }

    // Keyed: the transmitting band's TX pins, as before, whatever the
    // counted slices hold (no pin 7 on transmit); unkeyed the receive pins
    // of the highest slice again.
    void hl2Auto_keyed_txPinsAsBefore()
    {
        P1Session s(HPSDRHW::HermesLite);
        OcMatrix& oc = n2adrPins(s.model, s.conn);
        s.add(k160mHz);
        s.add(k40mHz);
        const quint8 unkeyed = quint8(oc.maskFor(Band::Band40m, /*tx=*/false) & ~kHighPassBit);
        QCOMPARE(p1OcByte(s.conn), unkeyed);
        s.conn.setTxFrequency(quint64(k160mHz));
        s.conn.setMox(true);
        QCOMPARE(p1OcByte(s.conn), oc.maskFor(Band::Band160m, /*tx=*/true));
        s.conn.setTxFrequency(quint64(k40mHz));
        QCOMPARE(p1OcByte(s.conn), oc.maskFor(Band::Band40m, /*tx=*/true));
        s.conn.setMox(false);
        QCOMPARE(p1OcByte(s.conn), unkeyed);
    }

    // mi0bot's band index (enums.cs [@c26a8a4]). NereusSDR uses it only
    // for mi0bot's range test (-1 sends no pins); the pins are ordered by
    // frequency.
    void extCtrlBandIndex_isMi0botsOrder()
    {
        QCOMPARE(OcMatrix::extCtrlBandIndex(Band::Band160m), 0);
        QCOMPARE(OcMatrix::extCtrlBandIndex(Band::Band6m), 10);
        QCOMPARE(OcMatrix::extCtrlBandIndex(Band::Band2m), 11);
        QCOMPARE(OcMatrix::extCtrlBandIndex(Band::WWV), 12);
        QCOMPARE(OcMatrix::extCtrlBandIndex(Band::Band120m), 28);
        QCOMPARE(OcMatrix::extCtrlBandIndex(Band::Band11m), 40);
        QCOMPARE(OcMatrix::extCtrlBandIndex(Band::GEN), -1);
        QCOMPARE(OcMatrix::extCtrlBandIndex(Band::XVTR), -1);
    }

    // ── CTUN at a low-pass edge: the reason names the filter's slice ─────
    //
    // Thetis picks the Alex low-pass from the DDS frequency, which under
    // click-tune is the DDC centre (console.cs:31894-31910 [v2.10.3.15]).
    // A sits at 16.55 MHz on a centre of 16.46 MHz (the 30/20 m row, which
    // ends at 16.5 MHz); B sits at 16.52 MHz on a centre of 16.53 MHz (the
    // 17/15 m row). By VFO A is higher; by centre B is. The filter follows
    // B's centre, and the reason must name B, with A and C (40 m) held.
    void ctun_reasonNamesTheSliceTheFilterFollows_data()
    {
        QTest::addColumn<bool>("protocol2");
        QTest::newRow("Protocol 1 (Hermes)") << false;
        QTest::newRow("Protocol 2 (G2)") << true;
    }
    void ctun_reasonNamesTheSliceTheFilterFollows()
    {
        QFETCH(bool, protocol2);
        constexpr double kVfoA = 16550000.0;
        constexpr double kCentreA = 16460000.0;
        constexpr double kFirstB = 16620000.0;   // outside A's window, so B has its own DDC
        constexpr double kVfoB = 16520000.0;
        constexpr double kCentreB = 16530000.0;

        std::unique_ptr<P1Session> p1;
        std::unique_ptr<G2Session> p2;
        RadioModel* model = nullptr;
        if (protocol2) {
            p2 = std::make_unique<G2Session>();
            model = &p2->model;
        } else {
            p1 = std::make_unique<P1Session>(HPSDRHW::Hermes);
            model = &p1->model;
        }
        const auto add = [&](double hz) { return protocol2 ? p2->add(hz) : p1->add(hz); };
        const auto lowPass = [&]() { return protocol2 ? p2Alex0Lpf(p2->conn) : p1LpfBits(p1->conn); };

        const int c = add(k40mHz);
        const int a = add(kVfoA);
        QVERIFY(model->requestStreamCtunPinned(a, true));
        QVERIFY(model->requestStreamCentre(a, kCentreA));
        const int b = add(kFirstB);
        QVERIFY(model->requestStreamCtunPinned(b, true));
        QVERIFY(model->requestStreamCentre(b, kCentreB));
        model->sliceById(b)->setFrequency(kVfoB);

        // The two orders disagree.
        const int streamA = model->sliceById(a)->streamIndex();
        const int streamB = model->sliceById(b)->streamIndex();
        QVERIFY(streamA >= 0 && streamB >= 0 && streamA != streamB);
        QCOMPARE(model->streamCentreHzForTest(streamA), kCentreA);
        QCOMPARE(model->streamCentreHzForTest(streamB), kCentreB);
        QVERIFY(model->sliceById(a)->frequency() > model->sliceById(b)->frequency());
        QCOMPARE(model->sliceChainIndex(a), 0);
        QCOMPARE(model->sliceChainIndex(b), 0);
        QCOMPARE(model->sliceChainIndex(c), 0);

        // The filter follows B's centre, and the reason names B.
        QCOMPARE(lowPass(), codec::alex::computeLpf(kCentreB / 1e6));
        QVERIFY(codec::alex::computeLpf(kCentreB / 1e6) != codec::alex::computeLpf(kCentreA / 1e6));
        const AlexController::AlexAdcState& st = model->filterChainState(0);
        QCOMPARE(st.lowPassSlice, b);
        const QString letterB = model->sliceById(b)->sliceLetter();
        const QString letterA = model->sliceById(a)->sliceLetter();
        QVERIFY2(st.lowPassReason.startsWith(
                     QStringLiteral("The receive low-pass filter is set for slice %1 ").arg(letterB)),
                 qPrintable(st.lowPassReason));
        QVERIFY2(st.lowPassReason.contains(QStringLiteral("%1 on ").arg(letterA)),
                 qPrintable(st.lowPassReason));
        QVERIFY2(st.lowPassReason.contains(letterOn(*model, c, "40m")), qPrintable(st.lowPassReason));
        QVERIFY2(OperatorWording::isPlain(st.lowPassReason), qPrintable(st.lowPassReason));
    }

    // ── HL2: a pin edit refreshes the held reason at once ────────────────
    //
    // The N2ADR pins are the HL2's receive low-pass. With 20 m and 17 m on
    // the same pin the two slices share one filter and nothing is held;
    // giving 17 m its own pin in Setup holds the 20 m slice behind the
    // 17 m filter straight away, with no slice or band change. In Auto
    // (JJ's ruling of 2026-09-30, no bypass for two masks) and with the
    // policy forcing a filter.
    void hl2_pinEditRefreshesTheHeldReason_data()
    {
        QTest::addColumn<bool>("forceBand");
        QTest::newRow("Auto") << false;
        QTest::newRow("Force band") << true;
    }
    void hl2_pinEditRefreshesTheHeldReason()
    {
        QFETCH(bool, forceBand);
        P1Session s(HPSDRHW::HermesLite);
        OcMatrix& oc = s.model.ocMatrixMutable();
        s.conn.setOcMatrix(&oc);
        oc.setPin(Band::Band20m, 0, /*tx=*/false, true);
        oc.setPin(Band::Band17m, 0, /*tx=*/false, true);
        if (forceBand) {
            s.model.alexControllerMutable().setBpfMode(0, AlexController::BpfMode::ForceBand);
        }

        const int a = s.add(k20mHz);
        const int b = s.add(k17mHz);
        QCOMPARE(s.model.sliceChainIndex(a), 0);
        QCOMPARE(s.model.sliceChainIndex(b), 0);
        QCOMPARE(s.model.filterChainState(0).lowPassSlice, -1);
        QVERIFY(s.model.filterChainState(0).lowPassReason.isEmpty());

        QSignalSpy changed(&s.model, &RadioModel::lowPassHoldChanged);
        oc.setPin(Band::Band17m, 1, /*tx=*/false, true);
        QCOMPARE(p1OcByte(s.conn), oc.maskFor(Band::Band17m, /*tx=*/false));
        QCOMPARE(s.model.filterChainState(0).effective, AlexController::BpfEffective::Filtered);
        QCOMPARE(s.model.filterChainState(0).lowPassSlice, b);
        QVERIFY2(s.model.filterChainState(0).lowPassReason.contains(letterOn(s.model, a, "20m")),
                 qPrintable(s.model.filterChainState(0).lowPassReason));
        QVERIFY(changed.count() > 0);

        // 17 m back on pin 0 alone: one filter again, nothing held.
        oc.setPin(Band::Band17m, 1, /*tx=*/false, false);
        QCOMPARE(s.model.filterChainState(0).lowPassSlice, -1);
        QVERIFY(s.model.filterChainState(0).lowPassReason.isEmpty());
    }

    // ── Keyed: the transmit low-pass, untouched ──────────────────────────
    void keyed_theCountedSetMovesNothing()
    {
        P1Session s(HPSDRHW::Hermes);
        s.add(k80mHz);
        s.conn.setTxFrequency(quint64(k40mHz));
        s.conn.setMox(true);
        const quint8 keyed = p1LpfBits(s.conn);
        QCOMPARE(keyed, codec::alex::computeLpf(k40mHz / 1e6));
        s.add(k20mHz);
        QCOMPARE(p1LpfBits(s.conn), keyed);
        s.conn.setMox(false);
        QCOMPARE(p1LpfBits(s.conn), codec::alex::computeLpf(k20mHz / 1e6));
    }
};

QTEST_MAIN(TestSharedInputFilters)
#include "tst_shared_input_filters.moc"
