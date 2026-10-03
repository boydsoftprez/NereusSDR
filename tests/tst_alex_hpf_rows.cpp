// no-port-check: test-only. Thetis file names appear only in source-cite
// comments that document which upstream line each assertion verifies.
// No Thetis logic is ported here; this file is NereusSDR-original.
//
// =================================================================
// The Alex Filters tabs' receive filter rows take effect.
// =================================================================
//
// Each row's Bypass, Start and End (Setup > Hardware > Alex-1 Filters: the
// high-pass ladder and the BPF1 bank; Alex-2 Filters: the Alex-2 bank and
// its master bypass) were saved and read by nothing. Thetis selects the
// receive high-pass from them:
//   From Thetis console.cs:6857-6870 [v2.10.3.15] (setAlexHPF)
//     if ((decimal)freq >= SetupForm.udAlex1_5HPFStart.Value && // 1.5 MHz HPF
//          (decimal)freq <= SetupForm.udAlex1_5HPFEnd.Value)
//     { if (alex1_5bphpf_bypass) NetworkIO.SetAlexHPFBits(0x20); // Bypass HPF
//       else NetworkIO.SetAlexHPFBits(0x10); ...
//   and no row: SetAlexHPFBits(0x20) (console.cs:6946-6950).
// setBPF1ForOrionIISaturn and setAlex2HPF (with alex2_hpf_bypass) have the
// same shape. The per-row setters re-select at once (console.cs:18823-18833
// Alex1_5BPHPFBypass { ... setAlex1HPF(freq); }).
// Radio-model radios with the Alex-2 bank: console.cs:15435-15444
// (UpdateRX2DDSFreq ... //DH1KLM).
// =================================================================

#include <QtTest/QtTest>

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QSignalSpy>

#include "core/AppSettings.h"
#include "core/P1RadioConnection.h"
#include "core/P2RadioConnection.h"
#include "core/RadioDiscovery.h"
#include "core/StepAttenuatorController.h"
#include "core/codec/AlexFilterMap.h"
#include "gui/setup/hardware/AntennaAlexAlex1Tab.h"
#include "gui/setup/hardware/AntennaAlexAlex2Tab.h"
#include "models/Band.h"
#include "models/RadioModel.h"

using namespace NereusSDR;
using namespace NereusSDR::codec::alex;

namespace {

const QString kMac = QStringLiteral("AA:BB:CC:DD:EE:26");
constexpr quint64 k40mHz = 7100000ULL;
constexpr quint8  kBypass = 0x20;

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

quint8 p1Hpf(const P1RadioConnection& conn)
{
    return quint8(conn.captureBank10ForTest()[3]) & 0x7F;
}

// The Alex0 HPF bits, the inverse of P2CodecOrionMkII::buildAlex0's scatter.
quint8 p2Hpf(P2RadioConnection& conn)
{
    quint8 buf[1444] = {};
    conn.composeCmdHighPriorityForTest(buf);
    const quint32 reg = readBE32(buf, 1432);
    quint8 bits = 0;
    if (reg & (1u << 1))  { bits |= 0x01; }
    if (reg & (1u << 2))  { bits |= 0x02; }
    if (reg & (1u << 4))  { bits |= 0x04; }
    if (reg & (1u << 5))  { bits |= 0x08; }
    if (reg & (1u << 6))  { bits |= 0x10; }
    if (reg & (1u << 12)) { bits |= 0x20; }
    if (reg & (1u << 3))  { bits |= 0x40; }
    return bits;
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

// A Core running the given radio on 6 m, for the 6 m LNA gain offset.
void prepareSixMeterCore(RadioModel& model, StepAttenuatorController& att,
                         HPSDRModel radio, RadioConnection* conn)
{
    model.setHpsdrModelForTest(radio);
    RadioInfo info;
    info.macAddress = kMac;
    info.boardType = boardForModel(radio);
    model.setLastRadioInfoForTest(info);
    model.setConnectionStateForTest(ConnectionState::Connected);
    model.injectConnectionForTest(conn);
    att.setTickTimerEnabled(false);
    model.setStepAttController(&att);
    att.setBand(Band::Band6m);
}

template <typename T>
T* named(QWidget& root, const QString& name)
{
    return root.findChild<T*>(name);
}

} // namespace

class TestAlexHpfRows : public QObject {
    Q_OBJECT

private slots:
    void init()    { AppSettings::instance().clearHardwareValues(kMac); }
    void cleanup() { AppSettings::instance().clearHardwareValues(kMac); }

    // ── The shipped rows are Thetis's spinner defaults ───────────────────
    void defaults_areThetisSpinnerValues()
    {
        const AlexHpfEdges d = AlexHpfEdges::thetisDefaults();
        // In-band selections the ladders always made.
        QCOMPARE(computeHpf(1.9), quint8(0x10));
        QCOMPARE(computeHpf(7.1), quint8(0x08));
        QCOMPARE(computeHpf(10.1), quint8(0x04));
        QCOMPARE(computeHpf(14.2), quint8(0x01));
        QCOMPARE(computeHpf(28.4), quint8(0x02));
        QCOMPARE(computeHpf(50.1), quint8(0x40));
        // Where Thetis differs from the old fixed ladder: the 1.5 MHz row
        // starts at 1.8 MHz (udAlex1_5HPFStart), and above 61.44 MHz no row
        // holds the frequency.
        QCOMPARE(computeHpf(1.6), kBypass);
        QCOMPARE(computeHpf(1.8), quint8(0x10));
        QCOMPARE(computeHpf(62.0), kBypass);
        // BPF1 crossovers are unchanged, hertz by hertz.
        QCOMPARE(computeBpf1(2.099999), quint8(0x10));
        QCOMPARE(computeBpf1(2.1), quint8(0x08));
        QCOMPARE(computeBpf1(61.44), quint8(0x40));
        QCOMPARE(computeBpf1(61.440001), kBypass);
        // The Alex-2 bank ships with the BPF1 edges, master off.
        QVERIFY(d.alex2 == d.bpf1);
        QVERIFY(!d.alex2Bypass);
        QCOMPARE(computeAlex2Hpf(7.1, d), quint8(0x04));
    }

    // ── A row's edges and bypass decide the selection ────────────────────
    void rows_edgesAndBypassSelect()
    {
        AlexHpfEdges e = AlexHpfEdges::thetisDefaults();
        // Move 7.1 MHz from the 6.5 MHz row into the 9.5 MHz row.
        e.hpf[1].endMhz = 6.999999;
        e.hpf[2].startMhz = 7.0;
        QCOMPARE(computeRxPreselector(7.1, HPSDRHW::Angelia, e), quint8(0x04));
        e.hpf[2].bypass = true;
        QCOMPARE(computeRxPreselector(7.1, HPSDRHW::Angelia, e), kBypass);
        // The band-pass board reads its own rows, not the ladder's.
        QCOMPARE(computeRxPreselector(7.1, HPSDRHW::Saturn, e), quint8(0x04));
        e.bpf1[2].bypass = true;
        QCOMPARE(computeRxPreselector(7.1, HPSDRHW::Saturn, e), kBypass);
        // The per-row 6 m bypass.
        QCOMPARE(computeRxPreselector(50.1, HPSDRHW::Angelia, e), quint8(0x40));
        e.hpf[5].bypass = true;
        QCOMPARE(computeRxPreselector(50.1, HPSDRHW::Angelia, e), kBypass);
        // A frequency no row holds is bypassed: the Alex-2 5.5 MHz row moved
        // to start at 7.2 MHz leaves 7.1 MHz outside every row.
        e.alex2[2].startMhz = 7.2;
        QCOMPARE(computeAlex2Hpf(7.1, e), kBypass);
    }

    void alex2_masterAndRows()
    {
        AlexHpfEdges e = AlexHpfEdges::thetisDefaults();
        e.alex2[2].bypass = true;
        QCOMPARE(computeAlex2Hpf(7.1, e), kBypass);
        e.alex2[2].bypass = false;
        QCOMPARE(computeAlex2Hpf(7.1, e), quint8(0x04));
        e.alex2Bypass = true;
        QCOMPARE(computeAlex2Hpf(7.1, e), kBypass);
        QCOMPARE(computeAlex2Hpf(14.2, e), kBypass);
        // The radios Thetis sets it on.
        QVERIFY(usesAlex2Hpf(HPSDRModel::ANAN_G2));
        QVERIFY(usesAlex2Hpf(HPSDRModel::ANAN7000D));
        QVERIFY(!usesAlex2Hpf(HPSDRModel::ANAN_G2E));
        QVERIFY(!usesAlex2Hpf(HPSDRModel::ANAN200D));
    }

    // ── Protocol 1 (Angelia), from the Alex-1 Filters tab ────────────────
    void p1_tabRowsReachBank10()
    {
        ConnectedP1 conn;
        conn.setBoardForTest(HPSDRHW::Angelia);
        conn.setReceiverFrequency(0, k40mHz);
        QCOMPARE(p1Hpf(conn), quint8(0x08));

        RadioModel model;
        prepareCore(model, HPSDRHW::Angelia, &conn);
        AntennaAlexAlex1Tab tab(&model);
        tab.restoreSettings(kMac);

        auto* bypass = named<QCheckBox>(tab, QStringLiteral("alexHpfBypass_6_5MHz"));
        auto* end65 = named<QDoubleSpinBox>(tab, QStringLiteral("alexHpfEnd_6_5MHz"));
        auto* start95 = named<QDoubleSpinBox>(tab, QStringLiteral("alexHpfStart_9_5MHz"));
        QVERIFY(bypass && end65 && start95);

        bypass->setChecked(true);
        QCOMPARE(p1Hpf(conn), kBypass);
        bypass->setChecked(false);
        QCOMPARE(p1Hpf(conn), quint8(0x08));

        // Edges: 7.1 MHz moves to the 9.5 MHz row.
        end65->setValue(6.999999);
        start95->setValue(7.0);
        QCOMPARE(p1Hpf(conn), quint8(0x04));
        QCOMPARE(model.alexHpfEdges().hpf[2].startMhz, 7.0);

        model.injectConnectionForTest(nullptr);
    }

    // ── Protocol 2 (G2), the BPF1 rows ───────────────────────────────────
    void p2_bpf1RowBypassReachesAlex0()
    {
        ConnectedP2 conn;
        conn.setBoardForTest(HPSDRHW::Saturn);
        conn.setReceiverFrequency(2, k40mHz);
        QCOMPARE(p2Hpf(conn), quint8(0x04));

        RadioModel model;
        prepareCore(model, HPSDRHW::Saturn, &conn);
        AntennaAlexAlex1Tab tab(&model);
        tab.restoreSettings(kMac);
        auto* bypass = named<QCheckBox>(tab, QStringLiteral("alexBpf1Bypass_9_5MHz"));
        QVERIFY(bypass);
        bypass->setChecked(true);
        QCOMPARE(p2Hpf(conn), kBypass);
        bypass->setChecked(false);
        QCOMPARE(p2Hpf(conn), quint8(0x04));
        model.injectConnectionForTest(nullptr);
    }

    // ── The Alex-2 tab's rows reach the model's edges ────────────────────
    void alex2Tab_rowsReachTheModel()
    {
        ConnectedP2 conn;
        conn.setBoardForTest(HPSDRHW::Saturn);
        RadioModel model;
        prepareCore(model, HPSDRHW::Saturn, &conn);
        AntennaAlexAlex2Tab tab(&model);
        tab.restoreSettings(kMac);
        auto* master = named<QCheckBox>(tab, QStringLiteral("alex2HpfBypass55"));
        auto* row = named<QCheckBox>(tab, QStringLiteral("alex2HpfBypass_13MHz"));
        QVERIFY(master && row);
        master->setChecked(true);
        QVERIFY(model.alexHpfEdges().alex2Bypass);
        QVERIFY(conn.alexHpfEdges().alex2Bypass);
        row->setChecked(true);
        QVERIFY(model.alexHpfEdges().alex2[3].bypass);
        model.injectConnectionForTest(nullptr);
    }

    // ── A remote window's change reaches the Core ────────────────────────
    void remoteWindowWrite_reachesTheCore()
    {
        ConnectedP2 conn;
        conn.setBoardForTest(HPSDRHW::Saturn);
        conn.setReceiverFrequency(2, k40mHz);
        RadioModel core;
        prepareCore(core, HPSDRHW::Saturn, &conn);
        QStringList reloads;
        core.setHardwareApplyObserverForTest([&reloads](const QString& name) { reloads << name; });

        const QString key = QStringLiteral("hardware/%1/alex/bpf1/9_5MHz/enabled").arg(kMac);
        AppSettings::instance().setValue(key, QStringLiteral("True"));
        core.scheduleRemoteHardwareApply(key);
        QTRY_COMPARE(reloads, QStringList{QStringLiteral("alex")});
        QCOMPARE(p2Hpf(conn), kBypass);

        reloads.clear();
        const QString master = QStringLiteral("hardware/%1/alex2/master/bypass55MhzBpf").arg(kMac);
        AppSettings::instance().setValue(master, QStringLiteral("True"));
        core.scheduleRemoteHardwareApply(master);
        QTRY_COMPARE(reloads, QStringList{QStringLiteral("alex")});
        QVERIFY(conn.alexHpfEdges().alex2Bypass);

        core.injectConnectionForTest(nullptr);
    }

    // ── A 6 m row's bypass reaches the receive calibration at once ───────
    // The 6 m LNA gain offset reads the 6 m rows' bypass (the Alex HPF row
    // on the ANAN-100D, the BPF1 row on the G2; tst_rx_6m_lna_offset has the
    // groups). A row change re-selects the high-pass at once, so the meters
    // and the spectrum take the offset at once too, not at the next band
    // change: rxMeterOffsetChanged is announced from the tab and from a
    // remote window's write reaching the Core.
    void sixMeterRowBypass_refreshesTheReceiveCalibration()
    {
        {
            ConnectedP1 conn;
            conn.setBoardForTest(HPSDRHW::Angelia);
            StepAttenuatorController att;
            RadioModel model;
            prepareSixMeterCore(model, att, HPSDRModel::ANAN100D, &conn);
            AntennaAlexAlex1Tab tab(&model);
            tab.restoreSettings(kMac);
            model.applyAlexHpfSwitchSettings();
            QCOMPARE(model.rx6mGainOffsetDb(), -13.0);

            QSignalSpy spy(&model, &RadioModel::rxMeterOffsetChanged);
            auto* bypass = named<QCheckBox>(tab, QStringLiteral("alexHpfBypass_6mBP"));
            QVERIFY(bypass);
            bypass->setChecked(true);
            QCOMPARE(spy.size(), 1);
            QCOMPARE(model.rx6mGainOffsetDb(), 0.0);
            QCOMPARE(spy.last().at(0).toDouble(), model.rxMeterOffsetDb());
            bypass->setChecked(false);
            QCOMPARE(spy.size(), 2);
            QCOMPARE(spy.last().at(0).toDouble(), model.rxMeterOffsetDb());

            model.setStepAttController(nullptr);
            model.injectConnectionForTest(nullptr);
        }
        AppSettings::instance().clearHardwareValues(kMac);
        {
            ConnectedP2 conn;
            conn.setBoardForTest(HPSDRHW::Saturn);
            StepAttenuatorController att;
            RadioModel core;
            prepareSixMeterCore(core, att, HPSDRModel::ANAN_G2, &conn);
            core.applyAlexHpfSwitchSettings();
            QCOMPARE(core.rx6mGainOffsetDb(), -13.0);
            QStringList reloads;
            core.setHardwareApplyObserverForTest(
                [&reloads](const QString& name) { reloads << name; });

            QSignalSpy spy(&core, &RadioModel::rxMeterOffsetChanged);
            const QString key = QStringLiteral("hardware/%1/alex/bpf1/6mBP/enabled").arg(kMac);
            AppSettings::instance().setValue(key, QStringLiteral("True"));
            core.scheduleRemoteHardwareApply(key);
            QTRY_COMPARE(reloads, QStringList{QStringLiteral("alex")});
            QCOMPARE(spy.size(), 1);
            QCOMPARE(core.rx6mGainOffsetDb(), 0.0);
            QCOMPARE(spy.last().at(0).toDouble(), core.rxMeterOffsetDb());

            core.setStepAttController(nullptr);
            core.injectConnectionForTest(nullptr);
        }
    }

    // ── A remote window whose Core cannot take them: disabled, with why ──
    void rows_closeWithTheReason()
    {
        RadioModel model(RadioModel::Role::Remote);
        AntennaAlexAlex1Tab tab1(&model);
        AntennaAlexAlex2Tab tab2(&model);
        const QString why = QStringLiteral("The Core cannot.");
        tab1.setHpfRowsAvailable(false, why);
        tab2.setHpfRowsAvailable(false, why);
        for (QWidget* w : {static_cast<QWidget*>(named<QCheckBox>(tab1, QStringLiteral("alexHpfBypass_1_5MHz"))),
                           static_cast<QWidget*>(named<QDoubleSpinBox>(tab1, QStringLiteral("alexBpf1Start_6mBP"))),
                           static_cast<QWidget*>(named<QCheckBox>(tab2, QStringLiteral("alex2HpfBypass55"))),
                           static_cast<QWidget*>(named<QDoubleSpinBox>(tab2, QStringLiteral("alex2HpfEnd_20MHz")))}) {
            QVERIFY(w);
            QVERIFY(!w->isEnabled());
            QCOMPARE(w->toolTip(), why);
        }
        tab1.setHpfRowsAvailable(true, why);
        QVERIFY(named<QCheckBox>(tab1, QStringLiteral("alexHpfBypass_1_5MHz"))->isEnabled());
    }
};

QTEST_MAIN(TestAlexHpfRows)
#include "tst_alex_hpf_rows.moc"
