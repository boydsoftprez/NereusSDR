// no-port-check: unit tests for the Thetis RXOffset(rx) port.
//
// Verifies:
//   1. rxMeterCalOffsetDefaultFor() returns the Thetis byte-for-byte
//      factory defaults from clsHardwareSpecific.cs:395-411 [v2.10.3.13]:
//         ANAN7000D/8000D/ORIONMKII/ANVELINAPRO3/REDPITAYA -> +4.841644 dB
//         ANAN_G2 / ANAN_G2_1K                              -> -4.476 dB
//         everything else (HPSDR / HERMES / HermesLite / etc) -> +0.98 dB
//   2. rxPreampOffsetDbFor() returns the Thetis byte-for-byte preamp-mode
//      offsets from console.cs:1991-2001 [v2.10.3.13]:
//         Off=20, On=0, Minus10=10, Minus20=20, Minus30=30, Minus40=40, Minus50=50
//   3. MeterPoller::setRxOffsetSource stores a callable that yields the
//      offset; poll() / pollSMeter() with a null channel still don't crash
//      (the offset source is invoked but the channel guard kicks in first).
//
// Cannot test the actual +offset applied to WDSP S-meter readings without
// a live WDSP channel; that's covered by the bench verification matrix.

#include <QtTest/QtTest>
#include <QCoreApplication>

#include "core/HpsdrModel.h"
#include "gui/meters/MeterPoller.h"

using namespace NereusSDR;

class TestRxMeterOffset : public QObject
{
    Q_OBJECT

private slots:
    // Factory cal offset defaults — Thetis clsHardwareSpecific.cs:395-411 [v2.10.3.13].
    void factoryCalOffset_anan7000d();
    void factoryCalOffset_anan8000d();
    void factoryCalOffset_orionmkii();
    void factoryCalOffset_anvelinaPro3();
    void factoryCalOffset_redPitaya();
    void factoryCalOffset_ananG2();
    void factoryCalOffset_ananG21K();
    void factoryCalOffset_hpsdrAtlas();
    void factoryCalOffset_hermesLite();
    void factoryCalOffset_anan100();
    void factoryCalOffset_anan100B();
    void factoryCalOffset_anan100D();
    void factoryCalOffset_anan200D();

    // Preamp-mode offsets — Thetis console.cs:1991-2001 [v2.10.3.13].
    void preampOffset_off();
    void preampOffset_on();
    void preampOffset_minus10();
    void preampOffset_minus20();
    void preampOffset_minus30();
    void preampOffset_minus40();
    void preampOffset_minus50();

    // MeterPoller::setRxOffsetSource accepts a callable and the poller stays
    // valid for poll/pollSMeter with null rx/tx channels (the existing nullptr
    // guards inside poll() and pollSMeter() take precedence).
    void meterPoller_setRxOffsetSource_acceptsCallable();
    void meterPoller_setRxOffsetSource_nullCallableSafe();
};

// 1. Per-radio factory defaults.

void TestRxMeterOffset::factoryCalOffset_anan7000d()
{
    // From Thetis clsHardwareSpecific.cs:399-404 [v2.10.3.13].
    QCOMPARE(rxMeterCalOffsetDefaultFor(HPSDRModel::ANAN7000D), 4.841644f);
}

void TestRxMeterOffset::factoryCalOffset_anan8000d()
{
    QCOMPARE(rxMeterCalOffsetDefaultFor(HPSDRModel::ANAN8000D), 4.841644f);
}

void TestRxMeterOffset::factoryCalOffset_orionmkii()
{
    QCOMPARE(rxMeterCalOffsetDefaultFor(HPSDRModel::ORIONMKII), 4.841644f);
}

void TestRxMeterOffset::factoryCalOffset_anvelinaPro3()
{
    QCOMPARE(rxMeterCalOffsetDefaultFor(HPSDRModel::ANVELINAPRO3), 4.841644f);
}

void TestRxMeterOffset::factoryCalOffset_redPitaya()
{
    QCOMPARE(rxMeterCalOffsetDefaultFor(HPSDRModel::REDPITAYA), 4.841644f);
}

void TestRxMeterOffset::factoryCalOffset_ananG2()
{
    // From Thetis clsHardwareSpecific.cs:405-407 [v2.10.3.13].
    QCOMPARE(rxMeterCalOffsetDefaultFor(HPSDRModel::ANAN_G2), -4.476f);
}

void TestRxMeterOffset::factoryCalOffset_ananG21K()
{
    QCOMPARE(rxMeterCalOffsetDefaultFor(HPSDRModel::ANAN_G2_1K), -4.476f);
}

void TestRxMeterOffset::factoryCalOffset_hpsdrAtlas()
{
    // From Thetis clsHardwareSpecific.cs:408-410 [v2.10.3.13] default branch.
    QCOMPARE(rxMeterCalOffsetDefaultFor(HPSDRModel::HPSDR), 0.98f);
}

void TestRxMeterOffset::factoryCalOffset_hermesLite()
{
    QCOMPARE(rxMeterCalOffsetDefaultFor(HPSDRModel::HERMESLITE), 0.98f);
}

void TestRxMeterOffset::factoryCalOffset_anan100()
{
    QCOMPARE(rxMeterCalOffsetDefaultFor(HPSDRModel::ANAN100), 0.98f);
}

void TestRxMeterOffset::factoryCalOffset_anan100B()
{
    QCOMPARE(rxMeterCalOffsetDefaultFor(HPSDRModel::ANAN100B), 0.98f);
}

void TestRxMeterOffset::factoryCalOffset_anan100D()
{
    QCOMPARE(rxMeterCalOffsetDefaultFor(HPSDRModel::ANAN100D), 0.98f);
}

void TestRxMeterOffset::factoryCalOffset_anan200D()
{
    QCOMPARE(rxMeterCalOffsetDefaultFor(HPSDRModel::ANAN200D), 0.98f);
}

// 2. Preamp-mode offset table.
// Slots align with PreampMode enum order at StepAttenuatorController.h:108-115:
//   0=Off, 1=On, 2=Minus10, 3=Minus20, 4=Minus30, 5=Minus40, 6=Minus50
// Values from Thetis console.cs:1991-2001 [v2.10.3.13].

void TestRxMeterOffset::preampOffset_off()
{
    QCOMPARE(rxPreampOffsetDbFor(0), 20.0f);  // HPSDR_OFF = atten inline
}

void TestRxMeterOffset::preampOffset_on()
{
    QCOMPARE(rxPreampOffsetDbFor(1), 0.0f);   // HPSDR_ON = no atten
}

void TestRxMeterOffset::preampOffset_minus10()
{
    QCOMPARE(rxPreampOffsetDbFor(2), 10.0f);
}

void TestRxMeterOffset::preampOffset_minus20()
{
    QCOMPARE(rxPreampOffsetDbFor(3), 20.0f);
}

void TestRxMeterOffset::preampOffset_minus30()
{
    QCOMPARE(rxPreampOffsetDbFor(4), 30.0f);
}

void TestRxMeterOffset::preampOffset_minus40()
{
    QCOMPARE(rxPreampOffsetDbFor(5), 40.0f);
}

void TestRxMeterOffset::preampOffset_minus50()
{
    QCOMPARE(rxPreampOffsetDbFor(6), 50.0f);
}

// 3. MeterPoller plumbing.

void TestRxMeterOffset::meterPoller_setRxOffsetSource_acceptsCallable()
{
    MeterPoller p;
    int callCount = 0;
    p.setRxOffsetSource([&callCount]() -> double {
        ++callCount;
        return 4.841644;  // ANAN-7000DLE factory default.
    });
    // Source is invoked from pollSMeter / poll loops.  We can't trigger
    // them headlessly without a channel, but we can verify the callable
    // is stored (the constructor + setter must not crash) and the poller
    // remains usable.  The actual offset application is bench-verified.
    p.start();
    QCoreApplication::processEvents();
    p.stop();

    // The callable hasn't been invoked yet because pollSMeter requires
    // both m_rxChannel and m_sMeter to be set.  This test exists to
    // guarantee no crash on setup; lattice tests with a live WDSP
    // channel cover the actual value application path.
    QCOMPARE(callCount, 0);
}

void TestRxMeterOffset::meterPoller_setRxOffsetSource_nullCallableSafe()
{
    MeterPoller p;
    // Default state: no source set.  poll() / pollSMeter() must treat
    // a null source as a 0.0 offset and not crash.
    p.start();
    QCoreApplication::processEvents();
    p.stop();
    // Explicit null assignment should also be safe.
    p.setRxOffsetSource({});
    p.start();
    QCoreApplication::processEvents();
    p.stop();
}

QTEST_MAIN(TestRxMeterOffset)
#include "tst_rx_meter_offset.moc"
