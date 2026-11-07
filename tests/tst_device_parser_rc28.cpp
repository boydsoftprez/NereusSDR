// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 13.
//                AI-assisted implementation via Anthropic Claude Code.
// no-port-check: NereusSDR-original test.
//
// Simulated RC-28 reports from the documented layout (R-HK-15, R-HK-08). A pass shows
// the code matches its sources, not what a device does.
#include <QtTest>

#include "core/hotkeys/HotKeyDeviceBindings.h"
#include "device_parser_test_util.h"
#include "gui/hotkeys/devices/Rc28Parser.h"

using namespace NereusSDR;
using parsertest::text;

namespace {

QByteArray report(quint8 buttons, quint8 direction = 0, quint8 speed = 0, quint8 guard = 0x01)
{
    QByteArray bytes(32, '\0');
    bytes[0] = static_cast<char>(guard);
    bytes[1] = static_cast<char>(speed);
    bytes[3] = static_cast<char>(direction);
    bytes[5] = static_cast<char>(buttons);
    return bytes;
}

} // namespace

class TestDeviceParserRc28 : public QObject {
    Q_OBJECT

private slots:
    void aPressAndItsRelease()
    {
        Rc28Parser parser;
        QVERIFY(parser.parse(report(0x07)).isEmpty());
        QCOMPARE(text(parser.parse(report(0x05))), QStringList{"down f1"});
        QCOMPARE(text(parser.parse(report(0x07))), QStringList{"up f1"});
    }

    void eachButtonHasItsOwnBit()
    {
        Rc28Parser parser;
        parser.parse(report(0x07));
        QCOMPARE(text(parser.parse(report(0x03))), QStringList{"down f2"});
        QCOMPARE(text(parser.parse(report(0x07))), QStringList{"up f2"});
        QCOMPARE(text(parser.parse(report(0x06))), QStringList{"down tx"});
        QCOMPARE(text(parser.parse(report(0x07))), QStringList{"up tx"});
    }

    void aHoldSeesItsReleaseWithAnotherButtonDown()
    {
        Rc28Parser parser;
        parser.parse(report(0x07));
        QCOMPARE(text(parser.parse(report(0x05))), QStringList{"down f1"});
        QCOMPARE(text(parser.parse(report(0x01))), QStringList{"down f2"});
        QCOMPARE(text(parser.parse(report(0x05))), QStringList{"up f2"});
        QCOMPARE(text(parser.parse(report(0x07))), QStringList{"up f1"});
    }

    void twoButtonsChangingInOneReportGiveBoth()
    {
        Rc28Parser parser;
        parser.parse(report(0x07));
        QCOMPARE(text(parser.parse(report(0x01))), (QStringList{"down f1", "down f2"}));
        QCOMPARE(text(parser.parse(report(0x07))), (QStringList{"up f1", "up f2"}));
    }

    void rotationIsReportedWithAButtonHeld()
    {
        Rc28Parser parser;
        parser.parse(report(0x07));
        QCOMPARE(text(parser.parse(report(0x06))), QStringList{"down tx"});
        QCOMPARE(text(parser.parse(report(0x06, 0x01, 3))), QStringList{"turn wheel 3"});
    }

    void aPressAndATurnInOneReportGiveBoth()
    {
        Rc28Parser parser;
        parser.parse(report(0x07));
        QCOMPARE(text(parser.parse(report(0x05, 0x01, 1))), (QStringList{"down f1", "turn wheel 1"}));
    }

    void rotationSignAndSpeed()
    {
        Rc28Parser parser;
        QCOMPARE(text(parser.parse(report(0x07, 0x02, 2))), QStringList{"turn wheel -2"});
        QCOMPARE(text(parser.parse(report(0x07, 0x01, 200))), QStringList{"turn wheel 200"});
        QVERIFY(parser.parse(report(0x07, 0x00, 2)).isEmpty());
        QVERIFY(parser.parse(report(0x07, 0x03, 2)).isEmpty());
        QVERIFY(parser.parse(report(0x07, 0x01, 0)).isEmpty());
    }

    void aWrongGuardByteGivesNothing()
    {
        Rc28Parser parser;
        parser.parse(report(0x07));
        QVERIFY(parser.parse(report(0x05, 0x01, 3, 0x02)).isEmpty());
        // And it changed no state: the next good report still shows the press.
        QCOMPARE(text(parser.parse(report(0x05))), QStringList{"down f1"});
    }

    void unknownHighBitsAreIgnored()
    {
        Rc28Parser parser;
        QVERIFY(parser.parse(report(0xF7)).isEmpty());
        QCOMPARE(text(parser.parse(report(0xF5))), QStringList{"down f1"});
        QVERIFY(parser.parse(report(0x85)).isEmpty());
    }

    void aShortReportGivesNothing()
    {
        Rc28Parser parser;
        QVERIFY(parser.parse(report(0x05).left(5)).isEmpty());
        QVERIFY(parser.parse(QByteArray()).isEmpty());
    }

    void resetForgetsWhatWasHeld()
    {
        Rc28Parser parser;
        parser.parse(report(0x07));
        parser.parse(report(0x05));
        parser.reset();
        QVERIFY(parser.parse(report(0x07)).isEmpty());
    }

    // ---- Connecting never keys ----

    // An open can bring guard 0x01 and byte 5 at 0x00, which reads as all three down.
    void aFirstReportWithEveryBitLowStartsNoPress()
    {
        Rc28Parser parser;
        QVERIFY(parser.parse(report(0x00)).isEmpty());
        QVERIFY(parser.parse(report(0x00)).isEmpty());
    }

    void aButtonSeenDownFirstPressesOnceItHasBeenSeenUp()
    {
        Rc28Parser parser;
        QVERIFY(parser.parse(report(0x06)).isEmpty());
        parser.parse(report(0x07));
        QCOMPARE(text(parser.parse(report(0x06))), QStringList{"down tx"});
    }

    void anotherButtonPressesWhileTheFirstReportsOneDown()
    {
        Rc28Parser parser;
        QVERIFY(parser.parse(report(0x06)).isEmpty());
        QCOMPARE(text(parser.parse(report(0x04))), QStringList{"down f1"});
    }

    void aTurnInTheFirstReportStillCounts()
    {
        Rc28Parser parser;
        QCOMPARE(text(parser.parse(report(0x00, 0x01, 2))), QStringList{"turn wheel 2"});
    }

    void aWrongGuardByteIsNotTheFirstReport()
    {
        Rc28Parser parser;
        QVERIFY(parser.parse(report(0x07, 0, 0, 0x02)).isEmpty());
        QVERIFY(parser.parse(report(0x06)).isEmpty());
    }

    void resetArmsTheFirstReportAgain()
    {
        Rc28Parser parser;
        parser.parse(report(0x07));
        QCOMPARE(text(parser.parse(report(0x06))), QStringList{"down tx"});
        parser.reset();
        QVERIFY(parser.parse(report(0x00)).isEmpty());
    }

    void lightReport()
    {
        const QByteArray txOnly = rc28LightReport(true, false, false, false);
        QCOMPARE(txOnly.size(), 33);
        QCOMPARE(static_cast<quint8>(txOnly[0]), quint8(0x00));
        QCOMPARE(static_cast<quint8>(txOnly[1]), quint8(0x01));
        QCOMPARE(static_cast<quint8>(txOnly[2]), quint8(0x0E));
        QCOMPARE(static_cast<quint8>(rc28LightReport(false, false, false, false)[2]), quint8(0x0F));
        QCOMPARE(static_cast<quint8>(rc28LightReport(false, true, false, false)[2]), quint8(0x0D));
        QCOMPARE(static_cast<quint8>(rc28LightReport(false, false, true, false)[2]), quint8(0x0B));
        QCOMPARE(static_cast<quint8>(rc28LightReport(false, false, false, true)[2]), quint8(0x07));
        QCOMPARE(static_cast<quint8>(rc28LightReport(true, true, true, true)[2]), quint8(0x00));
    }

    void staysMarkedUntested()
    {
        QVERIFY(!HotKeyDeviceBindings::testedOnRealDevice(DeviceId::Rc28));
    }
};

QTEST_GUILESS_MAIN(TestDeviceParserRc28)
#include "tst_device_parser_rc28.moc"
