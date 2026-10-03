// no-port-check: test-only. Thetis file names appear only in source-cite
// comments that document which upstream line each assertion verifies.
// No Thetis logic is ported here; this file is NereusSDR-original.
//
// =================================================================
// The Alex-1 Filters tab's low-pass rows take effect.
// =================================================================
//
// The seven LPF band rows (Setup > Hardware > Antenna/ALEX > Alex-1
// Filters) were saved, hidden and read by nothing: the low-pass came from a
// fixed ladder. Thetis selects it from them:
//   From Thetis console.cs:7177-7243 [v2.10.3.15] (setAlexLPF)
//     if (!_mox && lpf_bypass)
//     { NetworkIO.SetAlexLPFBits(0x10, false, _mox); // 6m LPF ... return; }
//     if (alexpresent && !initializing)
//     { if ((decimal)freq >= SetupForm.udAlex20mLPFStart.Value && // 30/20m LPF
//            (decimal)freq <= SetupForm.udAlex20mLPFEnd.Value)
//         NetworkIO.SetAlexLPFBits(0x01, freqIsTX, _mox); ...
//       else NetworkIO.SetAlexLPFBits(0x10, freqIsTX, _mox); // 6m LPF }
//   From Thetis ChannelMaster/netInterface.c:680-725 [v2.10.3.15]
//     if (isMox || isTX)  -> Alex1LPFMask;  if (isMox || !isTX) -> AlexLPFMask
// The spinner handlers keep the rows contiguous and do not re-select
// (setup.cs:15888-15994 [v2.10.3.15]).
//
// Nothing here keys a radio: every connection is an offline test object.
// =================================================================

#include <QtTest/QtTest>

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QMetaProperty>
#include <QSignalSpy>

#include <cmath>
#include <limits>
#include <vector>

#include "core/AppSettings.h"
#include "core/P1RadioConnection.h"
#include "core/P2RadioConnection.h"
#include "core/codec/AlexFilterMap.h"
#include "core/session/MirrorPolicy.h"
#include "gui/setup/hardware/AntennaAlexAlex1Tab.h"
#include "models/RadioModel.h"

using namespace NereusSDR;
using namespace NereusSDR::codec::alex;

namespace {

const QString kMac = QStringLiteral("AA:BB:CC:DD:EE:27");
constexpr quint8 k20m = 0x01;
constexpr quint8 k40m = 0x02;
constexpr quint8 k80m = 0x04;
constexpr quint8 k160m = 0x08;
constexpr quint8 k6m = 0x10;
constexpr quint8 k10m = 0x20;
constexpr quint8 k15m = 0x40;

class ConnectedP1 final : public P1RadioConnection {
public:
    ConnectedP1() { setState(ConnectionState::Connected); }
};

class ConnectedP2 final : public P2RadioConnection {
public:
    ConnectedP2() { setState(ConnectionState::Connected); }
};

quint32 readBE32(const quint8* buf, int offset)
{
    return (quint32(buf[offset])     << 24)
         | (quint32(buf[offset + 1]) << 16)
         | (quint32(buf[offset + 2]) << 8)
         |  quint32(buf[offset + 3]);
}

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

quint8 p1Lpf(const P1RadioConnection& conn)
{
    return quint8(conn.captureBank10ForTest()[4]) & 0x7F;
}

// Alex0 (bytes 1432-1435) and Alex1 (bytes 1428-1431) LPF masks.
AlexLpfMasks p2Lpf(P2RadioConnection& conn)
{
    quint8 buf[1444] = {};
    conn.composeCmdHighPriorityForTest(buf);
    return AlexLpfMasks{lpfMaskFromReg(readBE32(buf, 1432)),
                        lpfMaskFromReg(readBE32(buf, 1428))};
}

void prepareCore(RadioModel& model, HPSDRHW board, RadioConnection* conn)
{
    model.setBoardForTest(board);
    RadioInfo info;
    info.macAddress = kMac;
    info.boardType = board;
    model.setLastRadioInfoForTest(info);
    model.setConnectionStateForTest(ConnectionState::Connected);
    model.injectConnectionForTest(conn);
}

template <typename T>
T* named(QWidget& root, const QString& name)
{
    return root.findChild<T*>(name);
}

} // namespace

class TestAlexLpfRows : public QObject {
    Q_OBJECT

private slots:
    void init()    { AppSettings::instance().clearHardwareValues(kMac); }
    void cleanup() { AppSettings::instance().clearHardwareValues(kMac); }

    // ── The shipped rows are Thetis's spinner defaults ───────────────────
    // setup.designer.cs [v2.10.3.15]: udAlex160mLPFStart 0 / End 2.5,
    // 80m 2.500001 / 5, 40m 5.000001 / 8, 20m 8.000001 / 16.5,
    // 15m 16.500001 / 24, 10m 24.000001 / 35.6, 6m 35.600001 / 61.44.
    void defaults_areThetisSpinnerValues()
    {
        const AlexLpfEdges d = AlexLpfEdges::thetisDefaults();
        const double expect[kAlexLpfRowCount][2] = {
            {0.0, 2.5}, {2.500001, 5.0}, {5.000001, 8.0}, {8.000001, 16.5},
            {16.500001, 24.0}, {24.000001, 35.6}, {35.600001, 61.44},
        };
        for (int i = 0; i < kAlexLpfRowCount; ++i) {
            QCOMPARE(d.rows[size_t(i)].startMhz, expect[i][0]);
            QCOMPARE(d.rows[size_t(i)].endMhz, expect[i][1]);
        }
        QCOMPARE(QString::fromLatin1(kAlexLpfRowSlugs[0]), QStringLiteral("160m"));
        QCOMPARE(QString::fromLatin1(kAlexLpfRowSlugs[6]), QStringLiteral("6m"));
        // computeLpf is the selection over the defaults.
        QCOMPARE(computeLpf(14.2), k20m);
    }

    // ── Each band's start, end and one hertz below its start ─────────────
    void selection_eachBandEdge_data()
    {
        QTest::addColumn<double>("mhz");
        QTest::addColumn<int>("bits");
        QTest::newRow("160 start") << 0.0 << int(k160m);
        QTest::newRow("160 end") << 2.5 << int(k160m);
        QTest::newRow("80 start") << 2.500001 << int(k80m);
        QTest::newRow("80 end") << 5.0 << int(k80m);
        QTest::newRow("80 below") << 2.5 << int(k160m);
        QTest::newRow("40 start") << 5.000001 << int(k40m);
        QTest::newRow("40 end") << 8.0 << int(k40m);
        QTest::newRow("40 below") << 5.0 << int(k80m);
        QTest::newRow("20 start") << 8.000001 << int(k20m);
        QTest::newRow("20 end") << 16.5 << int(k20m);
        QTest::newRow("20 below") << 8.0 << int(k40m);
        QTest::newRow("15 start") << 16.500001 << int(k15m);
        QTest::newRow("15 end") << 24.0 << int(k15m);
        QTest::newRow("15 below") << 16.5 << int(k20m);
        QTest::newRow("10 start") << 24.000001 << int(k10m);
        QTest::newRow("10 end") << 35.6 << int(k10m);
        QTest::newRow("10 below") << 24.0 << int(k15m);
        QTest::newRow("6 start") << 35.600001 << int(k6m);
        QTest::newRow("6 end") << 61.44 << int(k6m);
        QTest::newRow("6 below") << 35.6 << int(k10m);
        // Where the defaults differ from the old fixed ladder.
        QTest::newRow("2.0 MHz is 160") << 2.0 << int(k160m);
        QTest::newRow("29.7 MHz is 10") << 29.7 << int(k10m);
        // Review I2: the transmit low-pass selection changed in these ranges
        // to match Thetis's spinner defaults (the old fixed ladder chose the
        // next filter up). Each range's low end, middle and top, and the
        // band tops 7.300, 14.350, 21.450 and 29.700 MHz.
        QTest::newRow("2.25 MHz is 160") << 2.25 << int(k160m);
        QTest::newRow("4.0 MHz is 80") << 4.0 << int(k80m);
        QTest::newRow("4.5 MHz is 80") << 4.5 << int(k80m);
        QTest::newRow("7.3 MHz is 40") << 7.3 << int(k40m);
        QTest::newRow("7.65 MHz is 40") << 7.65 << int(k40m);
        QTest::newRow("14.35 MHz is 20") << 14.35 << int(k20m);
        QTest::newRow("15.5 MHz is 20") << 15.5 << int(k20m);
        QTest::newRow("21.45 MHz is 15") << 21.45 << int(k15m);
        QTest::newRow("22.5 MHz is 15") << 22.5 << int(k15m);
        QTest::newRow("32.0 MHz is 10") << 32.0 << int(k10m);
    }
    void selection_eachBandEdge()
    {
        QFETCH(double, mhz);
        QFETCH(int, bits);
        QCOMPARE(int(selectAlexLpf(mhz, AlexLpfEdges::thetisDefaults())), bits);
        QCOMPARE(int(computeLpf(mhz)), bits);
    }

    // ── No row: the 6 m low-pass ─────────────────────────────────────────
    void selection_fallThroughIs6m()
    {
        AlexLpfEdges e = AlexLpfEdges::thetisDefaults();
        QCOMPARE(selectAlexLpf(61.440001, e), k6m);
        QCOMPARE(selectAlexLpf(144.0, e), k6m);
        // A gap between rows falls through too.
        e.rows[2].endMhz = 7.0;   // 40 m ends at 7 MHz
        QCOMPARE(selectAlexLpf(7.1, e), k6m);
    }

    // ── Thetis tests 20 m first: an overlap goes to the earlier test ─────
    void selection_thetisOrderWinsOverlaps()
    {
        AlexLpfEdges e = AlexLpfEdges::thetisDefaults();
        e.rows[3].startMhz = 1.0;   // 20 m row now spans 1 to 16.5 MHz
        QCOMPARE(selectAlexLpf(1.9, e), k20m);
        e = AlexLpfEdges::thetisDefaults();
        e.rows[4].startMhz = 30.0;  // 15 m (tested last) inside 10 m
        e.rows[4].endMhz = 31.0;
        QCOMPARE(selectAlexLpf(30.5, e), k10m);
        // Review M2: the rest of the order. 40 m before 80 m.
        e = AlexLpfEdges::thetisDefaults();
        e.rows[2].startMhz = 4.0;
        QCOMPARE(selectAlexLpf(4.5, e), k40m);
        // 80 m before 160 m.
        e = AlexLpfEdges::thetisDefaults();
        e.rows[1].startMhz = 1.8;
        QCOMPARE(selectAlexLpf(2.0, e), k80m);
        // 160 m before 6 m.
        e = AlexLpfEdges::thetisDefaults();
        e.rows[6].startMhz = 1.0;
        QCOMPARE(selectAlexLpf(2.0, e), k160m);
        // 6 m before 10 m.
        e = AlexLpfEdges::thetisDefaults();
        e.rows[6].startMhz = 34.0;
        QCOMPARE(selectAlexLpf(35.0, e), k6m);
    }

    // ── Review C1: each edge holds to its Thetis spinner's range ─────────
    // setup.designer.cs [v2.10.3.15], udAlex<band>LPFStart/End Minimum and
    // Maximum (kAlexLpfEdgeLimits).
    void limits_areThetisSpinnerRanges()
    {
        const double expect[kAlexLpfRowCount][4] = {
            {0.0, 1.999999, 1.5, 2.5},        {1.8, 2.999999, 3.0, 5.0},
            {4.0, 6.5, 6.500001, 8.0},        {7.0, 12.0, 12.000001, 16.5},
            {15.5, 21.0, 23.000001, 25.0},    {24.0, 30.0, 30.000001, 35.6},
            {34.0, 50.0, 50.000001, 61.44},
        };
        for (int i = 0; i < kAlexLpfRowCount; ++i) {
            const AlexLpfEdgeLimits& l = kAlexLpfEdgeLimits[size_t(i)];
            QCOMPARE(l.startMin, expect[i][0]);
            QCOMPARE(l.startMax, expect[i][1]);
            QCOMPARE(l.endMin, expect[i][2]);
            QCOMPARE(l.endMax, expect[i][3]);
            // Every default sits inside its range.
            const AlexLpfRow d = AlexLpfEdges::thetisDefaults().rows[size_t(i)];
            QVERIFY(alexLpfEdgeAllowed(i, false, d.startMhz));
            QVERIFY(alexLpfEdgeAllowed(i, true, d.endMhz));
        }
        QVERIFY(alexLpfEdgeAllowed(0, true, 2.5));
        QVERIFY(alexLpfEdgeAllowed(0, true, 1.5));
        QVERIFY(!alexLpfEdgeAllowed(0, true, 30.0));
        QVERIFY(!alexLpfEdgeAllowed(0, true, 1.499999));
        QVERIFY(!alexLpfEdgeAllowed(0, true, std::nan("")));
        QVERIFY(!alexLpfEdgeAllowed(0, true, std::numeric_limits<double>::infinity()));
        QVERIFY(!alexLpfEdgeAllowed(-1, true, 2.0));
        QVERIFY(!alexLpfEdgeAllowed(kAlexLpfRowCount, true, 2.0));
        QCOMPARE(clampAlexLpfEdge(0, true, 30.0, 2.5), 2.5);
        QCOMPARE(clampAlexLpfEdge(0, true, 1.0, 2.5), 1.5);
        QCOMPARE(clampAlexLpfEdge(0, true, std::nan(""), 2.2), 2.2);
        QCOMPARE(clampAlexLpfEdge(0, true, std::numeric_limits<double>::infinity(), 2.2), 2.2);
        QCOMPARE(clampAlexLpfEdge(4, true, 23.0, 24.0), 23.000001);
        QCOMPARE(clampAlexLpfEdge(6, true, 100.0, 61.44), 61.44);
    }

    // ── Review C1: 160m End set to 30, then a 25 MHz transmit ────────────
    // The tab's spinner holds 2.5 MHz, a saved 30 (or a value that is not a
    // number) reads as a value the spinner could hold, and the transmit
    // low-pass at 25 MHz stays the 10 m filter.
    void outOfRangeEdge_neverStoredOrApplied()
    {
        ConnectedP2 conn;
        conn.setBoardForTest(HPSDRHW::Saturn);
        conn.setTxFrequency(25000000ULL);
        conn.setReceiverFrequency(2, 7100000ULL);
        RadioModel model;
        prepareCore(model, HPSDRHW::Saturn, &conn);
        const QString endKey = QStringLiteral("alex/lpf/160m/end");
        auto& settings = AppSettings::instance();

        {
            AntennaAlexAlex1Tab tab(&model);
            tab.restoreSettings(kMac);
            auto* end160 = named<QDoubleSpinBox>(tab, QStringLiteral("alexLpfEnd_160m"));
            QVERIFY(end160);
            QCOMPARE(end160->minimum(), 1.5);
            QCOMPARE(end160->maximum(), 2.5);
            end160->setValue(30.0);
            QCOMPARE(end160->value(), 2.5);
            const QString stored = settings.hardwareValue(kMac, endKey).toString();
            QVERIFY2(stored.isEmpty() || stored.toDouble() == 2.5, qPrintable(stored));
        }

        // A saved value the spinner could not hold is not applied.
        for (const QString& raw : {QStringLiteral("30"), QStringLiteral("nan"),
                                   QStringLiteral("inf"), QStringLiteral("-inf")}) {
            settings.setHardwareValue(kMac, endKey, raw);
            QCOMPARE(RadioModel::savedAlexLpfEdges(kMac).rows[0].endMhz, 2.5);
            model.applyAlexHpfSwitchSettings();
            QCOMPARE(conn.alexLpfEdges().rows[0].endMhz, 2.5);
            QCOMPARE(p2Lpf(conn).alex1, k10m);
        }
        settings.setHardwareValue(kMac, endKey, QStringLiteral("1"));
        QCOMPARE(RadioModel::savedAlexLpfEdges(kMac).rows[0].endMhz, 1.5);

        // The tab shows the value the radio uses.
        settings.setHardwareValue(kMac, endKey, QStringLiteral("30"));
        AntennaAlexAlex1Tab tab(&model);
        tab.restoreSettings(kMac);
        QCOMPARE(named<QDoubleSpinBox>(tab, QStringLiteral("alexLpfEnd_160m"))->value(), 2.5);
        model.injectConnectionForTest(nullptr);
    }

    // ── Review I1: the neighbour rule, one implementation ────────────────
    // setup.cs:15888-15994 [v2.10.3.15]: each spinner's handler moves its
    // neighbour one hertz clear, and the moved spinner's own handler runs.
    void neighbourRule_cascadesAsThetisSpinnersDo()
    {
        AlexLpfRows rows = AlexLpfEdges::thetisDefaults().rows;
        // 80m Start down to 1.8: the 160m End moves below it.
        std::vector<AlexLpfEdgeMove> moved = applyAlexLpfEdgeEdit(rows, 1, false, 1.8);
        QCOMPARE(rows[1].startMhz, 1.8);
        QCOMPARE(moved.size(), size_t(1));
        QCOMPARE(moved[0], (AlexLpfEdgeMove{0, true, 1.799999}));
        QCOMPARE(rows[0].endMhz, 1.799999);

        // 160m Start up to 1.9: the 160m End moves above it, and the moved
        // End's handler moves the 80m Start above that.
        moved = applyAlexLpfEdgeEdit(rows, 0, false, 1.9);
        QCOMPARE(moved.size(), size_t(2));
        QCOMPARE(moved[0], (AlexLpfEdgeMove{0, true, 1.900001}));
        QCOMPARE(moved[1], (AlexLpfEdgeMove{1, false, 1.900002}));

        // An edit that clears its neighbours moves nothing.
        rows = AlexLpfEdges::thetisDefaults().rows;
        QVERIFY(applyAlexLpfEdgeEdit(rows, 3, true, 16.0).empty());
        // The 6m End has no handler.
        QVERIFY(applyAlexLpfEdgeEdit(rows, 6, true, 55.0).empty());
        // A neighbour held by its own range stops where the range does.
        rows = AlexLpfEdges::thetisDefaults().rows;
        moved = applyAlexLpfEdgeEdit(rows, 2, true, 8.0);
        QVERIFY(moved.empty());
        QVERIFY(!alexLpfNeighbourMove(rows, 2, true).has_value());
        // The edited edge is held to its range first.
        rows = AlexLpfEdges::thetisDefaults().rows;
        applyAlexLpfEdgeEdit(rows, 0, true, 30.0);
        QCOMPARE(rows[0].endMhz, 2.5);
    }

    // ── Review M1 + M3: a G2-class Core with a stored bypass ─────────────
    // Thetis unchecks 6m/ByPass on the boards that hide it; the Core saves
    // "False" once it knows the model, and the Alex0 word is the band's.
    void g2Core_storedBypassClearedAndAlex0Unchanged()
    {
        ConnectedP2 conn;
        conn.setBoardForTest(HPSDRHW::Saturn);
        conn.setTxFrequency(28400000ULL);
        conn.setReceiverFrequency(2, 7100000ULL);
        RadioModel model;
        prepareCore(model, HPSDRHW::Saturn, &conn);
        model.setHpsdrModelForTest(HPSDRModel::ANAN_G2);
        const QString key = QStringLiteral("alex/master/lpfBypass");
        AppSettings::instance().setHardwareValue(kMac, key, QStringLiteral("True"));
        model.applyAlexHpfSwitchSettings();
        const AlexLpfMasks w = p2Lpf(conn);
        QCOMPARE(w.alex0, k40m);
        QCOMPARE(w.alex1, k10m);
        QCOMPARE(AppSettings::instance().hardwareValue(kMac, key).toString(),
                 QStringLiteral("False"));
        model.injectConnectionForTest(nullptr);
    }

    // ── The mask writes: SetAlexLPFBits ──────────────────────────────────
    void maskWrites_followSetAlexLpfBits()
    {
        AlexLpfMasks m{k6m, k6m};
        QVERIFY(setAlexLpfBits(m, k40m, /*isTx=*/false, /*isMox=*/false));
        QCOMPARE(m.alex0, k40m);
        QCOMPARE(m.alex1, k6m);
        QVERIFY(setAlexLpfBits(m, k20m, true, false));
        QCOMPARE(m.alex0, k40m);
        QCOMPARE(m.alex1, k20m);
        QVERIFY(setAlexLpfBits(m, k80m, false, true));
        QCOMPARE(m.alex0, k80m);
        QCOMPARE(m.alex1, k80m);
        QVERIFY(!setAlexLpfBits(m, k80m, true, true));
    }

    // ── setAlexLPF: bypass, keyed and unkeyed, RX and TX, Alex present ───
    void setAlexLpf_bypassAndKeying()
    {
        const AlexLpfEdges d = AlexLpfEdges::thetisDefaults();
        AlexLpfMasks m{0, 0};
        // Unkeyed receive frequency: Alex0 only.
        setAlexLpf(m, 7.1, false, false, false, true, d);
        QCOMPARE(m.alex0, k40m);
        QCOMPARE(m.alex1, quint8(0));
        // Unkeyed transmit frequency: Alex1 only.
        setAlexLpf(m, 28.4, true, false, false, true, d);
        QCOMPARE(m.alex0, k40m);
        QCOMPARE(m.alex1, k10m);
        // Keyed: both, whichever frequency.
        setAlexLpf(m, 14.2, true, true, false, true, d);
        QCOMPARE(m.alex0, k20m);
        QCOMPARE(m.alex1, k20m);
        // Bypass unkeyed: 6 m on Alex0 only, from either call.
        m = AlexLpfMasks{k40m, k10m};
        setAlexLpf(m, 7.1, false, false, true, true, d);
        QCOMPARE(m.alex0, k6m);
        QCOMPARE(m.alex1, k10m);
        m = AlexLpfMasks{k40m, k10m};
        setAlexLpf(m, 28.4, true, false, true, true, d);
        QCOMPARE(m.alex0, k6m);
        QCOMPARE(m.alex1, k10m);
        // Bypass keyed: no effect, the band's filter on both.
        setAlexLpf(m, 3.7, true, true, true, true, d);
        QCOMPARE(m.alex0, k80m);
        QCOMPARE(m.alex1, k80m);
        // No Alex: no write, except the bypass, which Thetis tests first.
        m = AlexLpfMasks{0, 0};
        QVERIFY(!setAlexLpf(m, 7.1, false, false, false, false, d));
        QCOMPARE(m.alex0, quint8(0));
        QVERIFY(setAlexLpf(m, 7.1, false, false, true, false, d));
        QCOMPARE(m.alex0, k6m);
    }

    // ── Protocol 1: bank 10 C4, receive vs transmit, keyed and unkeyed ───
    void p1_rxAndTxKeyedAndUnkeyed()
    {
        ConnectedP1 conn;
        conn.setBoardForTest(HPSDRHW::Angelia);
        conn.setTxFrequency(28400000ULL);
        conn.setReceiverFrequency(0, 7100000ULL);
        QCOMPARE(p1Lpf(conn), k40m);            // unkeyed: the receive filter
        conn.setMox(true);
        QCOMPARE(p1Lpf(conn), k10m);            // keyed: the transmit filter
        conn.setReceiverFrequency(0, 3700000ULL);
        QCOMPARE(p1Lpf(conn), k10m);            // a receive retune while keyed
        conn.setMox(false);
        QCOMPARE(p1Lpf(conn), k80m);            // back to the receive filter
        QCOMPARE(conn.alexLpfBitsInUse(), int(k80m));
    }

    void p1_bypassAndEdges()
    {
        ConnectedP1 conn;
        conn.setBoardForTest(HPSDRHW::Angelia);
        conn.setTxFrequency(28400000ULL);
        conn.setReceiverFrequency(0, 7100000ULL);
        conn.setAlexLpfBypass(true);
        QCOMPARE(p1Lpf(conn), k6m);
        conn.setMox(true);
        QCOMPARE(p1Lpf(conn), k10m);            // bypass is receive-only
        conn.setMox(false);
        QCOMPARE(p1Lpf(conn), k6m);
        conn.setAlexLpfBypass(false);
        QCOMPARE(p1Lpf(conn), k40m);

        // An edge change is read by the next selection, not applied at once.
        AlexLpfEdges e = AlexLpfEdges::thetisDefaults();
        e.rows[2].endMhz = 7.0;
        e.rows[3].startMhz = 7.000001;
        conn.setAlexLpfEdges(e);
        QCOMPARE(p1Lpf(conn), k40m);
        conn.setReceiverFrequency(0, 7100001ULL);
        QCOMPARE(p1Lpf(conn), k20m);
    }

    // ── Protocol 2: Alex0 and Alex1 words ────────────────────────────────
    void p2_alex0AndAlex1()
    {
        ConnectedP2 conn;
        conn.setBoardForTest(HPSDRHW::Saturn);
        conn.setTxFrequency(28400000ULL);
        conn.setReceiverFrequency(2, 7100000ULL);
        AlexLpfMasks w = p2Lpf(conn);
        QCOMPARE(w.alex0, k40m);
        QCOMPARE(w.alex1, k10m);
        conn.setMox(true);
        w = p2Lpf(conn);
        QCOMPARE(w.alex0, k10m);
        QCOMPARE(w.alex1, k10m);
        conn.setMox(false);
        w = p2Lpf(conn);
        QCOMPARE(w.alex0, k40m);
        QCOMPARE(w.alex1, k10m);
        conn.setAlexLpfBypass(true);
        w = p2Lpf(conn);
        QCOMPARE(w.alex0, k6m);
        QCOMPARE(w.alex1, k10m);
        QCOMPARE(conn.alexLpfBitsInUse(), int(k6m));
    }

    // ── An edge edit while keyed is stored, not applied ──────────────────
    void p2_edgeEditWhileKeyed_notApplied()
    {
        ConnectedP2 conn;
        conn.setBoardForTest(HPSDRHW::Saturn);
        conn.setTxFrequency(14200000ULL);
        conn.setReceiverFrequency(2, 14200000ULL);
        conn.setMox(true);
        AlexLpfEdges e = AlexLpfEdges::thetisDefaults();
        e.rows[3].endMhz = 14.0;     // 20 m now ends below 14.2 MHz
        e.rows[4].startMhz = 14.000001;
        conn.setAlexLpfEdges(e);
        QCOMPARE(p2Lpf(conn).alex1, k20m);   // the keyed filter is unchanged
        conn.setMox(false);
        QCOMPARE(p2Lpf(conn).alex0, k15m);   // read by the next selection
        QCOMPARE(p2Lpf(conn).alex1, k15m);
    }

    // ── The tab: rows shown, saved per radio, bypass re-applies ──────────
    void tab_rowsSaveAndBypassApplies()
    {
        ConnectedP1 conn;
        conn.setBoardForTest(HPSDRHW::Angelia);
        conn.setReceiverFrequency(0, 7100000ULL);
        RadioModel model;
        prepareCore(model, HPSDRHW::Angelia, &conn);
        model.wireBandOutputsReportForTest();
        AntennaAlexAlex1Tab tab(&model);
        tab.restoreSettings(kMac);

        auto* end40 = named<QDoubleSpinBox>(tab, QStringLiteral("alexLpfEnd_40m"));
        auto* start20 = named<QDoubleSpinBox>(tab, QStringLiteral("alexLpfStart_20m"));
        auto* bypass = named<QCheckBox>(tab, QStringLiteral("alexLpfBypass"));
        QVERIFY(end40 && start20 && bypass);
        QVERIFY(!end40->isHidden());
        QCOMPARE(end40->property("nereusSetupId").toString(),
                 QStringLiteral("hardware.alex1Filters.lpf.40m.end"));

        bypass->setChecked(true);
        QCOMPARE(AppSettings::instance().hardwareValue(
                     kMac, QStringLiteral("alex/master/lpfBypass")).toString(),
                 QStringLiteral("True"));
        QCOMPARE(p1Lpf(conn), k6m);
        bypass->setChecked(false);
        QCOMPARE(p1Lpf(conn), k40m);

        // Thetis's udAlex20mLPFStart handler moves the 40 m end below it.
        // (The 40 m end cannot pass 8 MHz, its spinner's maximum.)
        end40->setValue(8.5);
        QCOMPARE(end40->value(), 8.0);
        start20->setValue(7.5);
        QCOMPARE(end40->value(), 7.499999);
        QCOMPARE(AppSettings::instance().hardwareValue(
                     kMac, QStringLiteral("alex/lpf/40m/end")).toString().toDouble(),
                 7.499999);
        QCOMPARE(model.alexLpfEdges().rows[2].endMhz, 7.499999);
        QCOMPARE(model.alexLpfEdges().rows[3].startMhz, 7.5);

        // The indicator is the filter in use.
        QTRY_COMPARE(model.alexLpfBits(), int(k40m));
        model.injectConnectionForTest(nullptr);
    }

    // ── A remote window's change reaches the Core ────────────────────────
    void remoteWindowWrite_reachesTheCore()
    {
        // ANAN-100D on P2: the bypass is available (not a G2-class board),
        // so a stored True applies.
        ConnectedP2 conn;
        conn.setBoardForTest(HPSDRHW::Angelia);
        conn.setReceiverFrequency(2, 7100000ULL);
        RadioModel core;
        prepareCore(core, HPSDRHW::Angelia, &conn);
        QStringList reloads;
        core.setHardwareApplyObserverForTest([&reloads](const QString& name) { reloads << name; });

        const QString key = QStringLiteral("hardware/%1/alex/master/lpfBypass").arg(kMac);
        AppSettings::instance().setValue(key, QStringLiteral("True"));
        core.scheduleRemoteHardwareApply(key);
        QTRY_COMPARE(reloads, QStringList{QStringLiteral("alex")});
        QCOMPARE(p2Lpf(conn).alex0, k6m);

        reloads.clear();
        const QString edge = QStringLiteral("hardware/%1/alex/lpf/40m/end").arg(kMac);
        AppSettings::instance().setValue(edge, QStringLiteral("7"));
        core.scheduleRemoteHardwareApply(edge);
        QTRY_COMPARE(reloads, QStringList{QStringLiteral("alex")});
        QCOMPARE(conn.alexLpfEdges().rows[2].endMhz, 7.0);

        core.injectConnectionForTest(nullptr);
    }

    // ── A remote window whose Core cannot take them: disabled, with why ──
    void rows_closeWithTheReason()
    {
        RadioModel model(RadioModel::Role::Remote);
        AntennaAlexAlex1Tab tab(&model);
        const QString why = QStringLiteral("The Core cannot.");
        tab.setLpfRowsAvailable(false, why);
        for (QWidget* w : {static_cast<QWidget*>(named<QDoubleSpinBox>(tab, QStringLiteral("alexLpfStart_160m"))),
                           static_cast<QWidget*>(named<QDoubleSpinBox>(tab, QStringLiteral("alexLpfEnd_6m"))),
                           static_cast<QWidget*>(named<QCheckBox>(tab, QStringLiteral("alexLpfBypass")))}) {
            QVERIFY(w);
            QVERIFY(!w->isEnabled());
            QCOMPARE(w->toolTip(), why);
        }
        tab.setLpfRowsAvailable(true, why);
        QVERIFY(named<QDoubleSpinBox>(tab, QStringLiteral("alexLpfStart_160m"))->isEnabled());
    }

    // ── The radios Thetis hides 6m/ByPass on: shown disabled, with why ───
    void bypass_disabledOnTheRadiosThetisHidesIt()
    {
        ConnectedP2 conn;
        conn.setBoardForTest(HPSDRHW::Saturn);
        RadioModel model;
        prepareCore(model, HPSDRHW::Saturn, &conn);
        model.setHpsdrModelForTest(HPSDRModel::ANAN_G2);
        AntennaAlexAlex1Tab tab(&model);
        tab.restoreSettings(kMac);
        auto* bypass = named<QCheckBox>(tab, QStringLiteral("alexLpfBypass"));
        QVERIFY(bypass);
        QVERIFY(!bypass->isHidden());
        QVERIFY(!bypass->isEnabled());
        QVERIFY(!bypass->isChecked());
        QVERIFY(!bypass->toolTip().isEmpty());
        model.injectConnectionForTest(nullptr);
    }

    // ── Older windows keep today's wire ──────────────────────────────────
    // The lamp's source is read-only, reaches only a peer that declared
    // alexLpf, and is declared after every earlier property so no earlier
    // property's ordinal moves.
    void lampSource_readOnlyGatedAndLast()
    {
        const QMetaObject& meta = RadioModel::staticMetaObject;
        const int index = meta.indexOfProperty("alexLpfBits");
        QVERIFY(index >= 0);
        const QMetaProperty property = meta.property(index);
        QVERIFY(!property.isWritable());
        QVERIFY(property.hasNotifySignal());
        // Only paTransmitBand (paTransmitBandVersion, appended after it),
        // the four Level Cal run properties and the shared-input low-pass
        // reason and slice (rxFilterLowPassVersion) follow it.
        QCOMPARE(index, meta.propertyCount() - 8);
        QCOMPARE(meta.indexOfProperty("paTransmitBand"), meta.propertyCount() - 7);
        QCOMPARE(meta.indexOfProperty("levelCalSucceeded"), meta.propertyCount() - 3);
        QCOMPARE(meta.indexOfProperty("rxFilter0LowPassReason"), meta.propertyCount() - 2);
        QCOMPARE(meta.indexOfProperty("rxFilter0LowPassSlice"), meta.propertyCount() - 1);
        QCOMPARE(MirrorPolicy::directionFor(QByteArrayLiteral("RadioModel"), "alexLpfBits"),
                 MirrorDirection::Outbound);
        const MirrorPolicy::FeatureGate* gate =
            MirrorPolicy::featureGateFor(QByteArrayLiteral("RadioModel"), "alexLpfBits");
        QVERIFY(gate != nullptr);
        QCOMPARE(QByteArray(gate->feature), QByteArrayLiteral("alexLpf"));

        RadioModel model;
        QCOMPARE(model.alexLpfBits(), -1);
    }
};

QTEST_MAIN(TestAlexLpfRows)
#include "tst_alex_lpf_rows.moc"
