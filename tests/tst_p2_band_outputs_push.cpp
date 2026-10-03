// no-port-check: test-only. Thetis file names appear only in source-cite
// comments that document which upstream line each assertion verifies.
// No Thetis logic is ported here; this file is NereusSDR-original.
//
// =================================================================
// Protocol 2 sends the band outputs when, and only when, they change.
// =================================================================
//
// Plan Task 14 fix wave, M2 and M3 (3M-1 transmit). Thetis sends a
// high-priority packet for the OC bits exactly when they change:
//   From Thetis ChannelMaster/netInterface.c:399-407 [v2.10.3.15]
//     void SetOCBits(int b)
//     { if (prn->oc_output != b)
//       { prn->oc_output = b;
//         if (listenSock != INVALID_SOCKET && prn->sendHighPriority != 0)
//             CmdHighPriority(); } }
// and a pin edit reaches it at once:
//   From Thetis setup.cs:12718 [v2.10.3.15]
//     console.PennyExtCtrlEnabled = chkPennyExtCtrl.Checked;  // need side effect of this to push change to native code
//
// M2: a pin edit did not reach the radio until something else sent a
// high-priority packet (the Protocol 2 heartbeat runs only while keyed).
// M3: every VFO step sent a high-priority packet, whether or not the OC
// byte changed.
//
// Against the fake Protocol 2 radio (an Orion MkII), over loopback.
// =================================================================

#include <QtTest/QtTest>

#include <functional>

#include "core/AppSettings.h"
#include "core/OcMatrix.h"
#include "core/P2RadioConnection.h"
#include "fakes/P2FakeRadio.h"
#include "models/Band.h"
#include "models/RadioModel.h"

using namespace NereusSDR;
using NereusSDR::Test::P2FakeRadio;

namespace {

constexpr quint64 k20mHz  = 14200000ULL;
constexpr quint64 k20mHz2 = 14201000ULL;
constexpr quint64 k40mHz  =  7100000ULL;
constexpr quint64 k17mHz  = 18100000ULL;
constexpr quint64 k15mHz  = 21200000ULL;

// A quiet interval long enough for a queued push to have gone out.
constexpr int kQuietMs = 150;

bool waitUntil(const std::function<bool()>& predicate, int timeoutMs = 3000)
{
    return QTest::qWaitFor(predicate, timeoutMs);
}

// A Core model with the connection's pin matrix, and a Protocol 2 connection
// up against the fake radio, receiving on DDC2 (slice A on the G2 layout).
struct P2Core {
    P2Core()
    {
        AppSettings::instance().clear();
        QVERIFY(fake.start());
        RadioInfo info = fake.radioInfo();
        model.setBoardForTest(HPSDRHW::OrionMKII);
        model.setLastRadioInfoForTest(info);
        m = &model.ocMatrixMutable();
        m->setMacAddress(info.macAddress);
        m->setPin(Band::Band20m, 0, /*tx=*/false, true);
        m->setPin(Band::Band40m, 1, /*tx=*/false, true);

        conn.setPortBasesForTest(fake.outboundPortBase(), fake.inputRolePortBase());
        conn.init();
        conn.setOcMatrix(m);
        conn.connectToRadio(info);
        QVERIFY(waitUntil([this]() { return fake.hasClient(); }));
        fake.sendDdc(2);
        QVERIFY(waitUntil([this]() { return conn.state() == ConnectionState::Connected; }));

        model.injectConnectionForTest(&conn);
        model.wireBandOutputsReportForTest();
        conn.setLiveReceiverSlots(1u << 2);
        conn.setReceiverVfoFrequencies(vfoOnDdc2(k20mHz));
        QVERIFY(waitUntil([this]() { return fake.lastHighPriorityOcByte() == 0x01; }));
        QTest::qWait(kQuietMs);
    }
    ~P2Core()
    {
        model.injectConnectionForTest(nullptr);
        conn.disconnect();
        fake.stop();
        AppSettings::instance().clear();
    }

    static QVector<quint64> vfoOnDdc2(quint64 hz)
    {
        QVector<quint64> v(3, 0);
        v[2] = hz;
        return v;
    }

    P2FakeRadio       fake;
    RadioModel        model;
    P2RadioConnection conn;
    OcMatrix*         m{nullptr};
};

} // namespace

class TestP2BandOutputsPush : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        AppSettings::setProfileOverride(
            QStringLiteral("p2-band-outputs-push-%1").arg(QCoreApplication::applicationPid()));
        AppSettings::instance().clear();
    }
    void cleanupTestCase()
    {
        const QString path = AppSettings::instance().filePath();
        QFile::remove(path);
        QFile::remove(path + QStringLiteral(".bak"));
    }

    // ── M2: a pin edit reaches the radio at once ─────────────────────────
    void pinEdit_isSentAtOnce()
    {
        P2Core core;
        const int before = core.fake.highPriorityDatagrams();

        core.m->setPin(Band::Band20m, 2, /*tx=*/false, true);
        QVERIFY(waitUntil([&core]() { return core.fake.lastHighPriorityOcByte() == 0x05; }, 500));
        QCOMPARE(core.fake.highPriorityDatagrams(), before + 1);

        // A pin on another band leaves the byte alone: nothing is sent.
        core.m->setPin(Band::Band80m, 3, /*tx=*/false, true);
        QTest::qWait(kQuietMs);
        QCOMPARE(core.fake.highPriorityDatagrams(), before + 1);
    }

    // ── M2: a remote window's pin edit (the "oc" reload) does too ────────
    void remoteWindowPinEdit_isSentAtOnce()
    {
        P2Core core;
        core.m->save();
        const int before = core.fake.highPriorityDatagrams();

        // What the window's OcMatrix::save leaves in the Core's settings:
        // the same matrix with 20 m's pin 6 added.
        OcMatrix window;
        window.setMacAddress(core.fake.radioInfo().macAddress);
        window.load();
        window.setPin(Band::Band20m, 6, /*tx=*/false, true);
        window.save();

        core.model.scheduleRemoteHardwareApply(
            QStringLiteral("hardware/%1/oc/rx/20m/6").arg(core.fake.radioInfo().macAddress));
        QVERIFY(waitUntil([&core]() { return core.fake.lastHighPriorityOcByte() == 0x41; }, 1000));
        QCOMPARE(core.fake.highPriorityDatagrams(), before + 1);
    }

    // ── M3: a VFO step sends only when the OC byte changes ───────────────
    void vfoStep_sendsOnlyWhenTheByteChanges()
    {
        P2Core core;
        const int start = core.fake.highPriorityDatagrams();

        // Inside 20 m: the byte is unchanged, nothing is sent.
        core.conn.setReceiverVfoFrequencies(P2Core::vfoOnDdc2(k20mHz2));
        QTest::qWait(kQuietMs);
        QCOMPARE(core.fake.highPriorityDatagrams(), start);

        // Into 40 m: a new byte, one packet.
        core.conn.setReceiverVfoFrequencies(P2Core::vfoOnDdc2(k40mHz));
        QVERIFY(waitUntil([&core]() { return core.fake.lastHighPriorityOcByte() == 0x02; }, 500));
        QCOMPARE(core.fake.highPriorityDatagrams(), start + 1);

        // 17 m then 15 m: neither has pins, so 0 then 0 again. One packet
        // for the change to 0, none for the band change that keeps it.
        core.conn.setReceiverVfoFrequencies(P2Core::vfoOnDdc2(k17mHz));
        QVERIFY(waitUntil([&core]() { return core.fake.lastHighPriorityOcByte() == 0x00; }, 500));
        QCOMPARE(core.fake.highPriorityDatagrams(), start + 2);
        core.conn.setReceiverVfoFrequencies(P2Core::vfoOnDdc2(k15mHz));
        QTest::qWait(kQuietMs);
        QCOMPARE(core.fake.highPriorityDatagrams(), start + 2);

        // The band shown with the byte still follows (RadioModel).
        QTRY_COMPARE(core.model.bandOutputsBand(), int(Band::Band15m));
        QCOMPARE(core.model.bandOutputsByte(), 0);
    }
};

QTEST_MAIN(TestP2BandOutputsPush)
#include "tst_p2_band_outputs_push.moc"
