// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 14.
//                AI-assisted implementation via Anthropic Claude Code.
// no-port-check: NereusSDR-original test.
//
// Simulated TMate 2 reports from the documented layout (R-HK-15). A pass shows the code
// matches its sources, not what a device does.
#include <QtTest>

#include "core/hotkeys/HotKeyDeviceBindings.h"
#include "device_parser_test_util.h"
#include "gui/hotkeys/devices/TMate2Parser.h"

using namespace NereusSDR;
using parsertest::text;

namespace {

constexpr quint16 kIdle = 0x01FF;

QByteArray report(quint16 enc0, quint16 enc1, quint16 enc2, quint16 keys = kIdle, quint8 id = 0x01)
{
    QByteArray bytes(64, '\0');
    bytes[0] = static_cast<char>(id);
    const auto put = [&bytes](int at, quint16 value) {
        bytes[at] = static_cast<char>(value & 0xFF);
        bytes[at + 1] = static_cast<char>(value >> 8);
    };
    put(1, enc0);
    put(3, enc1);
    put(5, enc2);
    put(7, keys);
    return bytes;
}

} // namespace

class TestDeviceParserTMate2 : public QObject {
    Q_OBJECT

private slots:
    void theFirstReportOnlySeeds()
    {
        TMate2Parser parser;
        QVERIFY(parser.parse(report(0x1234, 0x5678, 0x9ABC, 0x01FE)).isEmpty());
        QVERIFY(parser.parse(report(0x1234, 0x5678, 0x9ABC, 0x01FE)).isEmpty());
    }

    void encoderOneIsPrintedE2()
    {
        TMate2Parser parser;
        parser.parse(report(5, 5, 5));
        QCOMPARE(text(parser.parse(report(5, 7, 5))), QStringList{"turn e2 2"});
    }

    void encoderZeroIsMainAndNegated()
    {
        TMate2Parser parser;
        parser.parse(report(5, 5, 5));
        QCOMPARE(text(parser.parse(report(7, 5, 5))), QStringList{"turn main -2"});
    }

    void encoderTwoIsPrintedE1()
    {
        TMate2Parser parser;
        parser.parse(report(5, 5, 5));
        QCOMPARE(text(parser.parse(report(5, 5, 4))), QStringList{"turn e1 -1"});
    }

    void aCounterWrapsBothWays()
    {
        TMate2Parser parser;
        parser.parse(report(0, 0xFFFF, 0));
        QCOMPARE(text(parser.parse(report(0, 0x0001, 0))), QStringList{"turn e2 2"});
        QCOMPARE(text(parser.parse(report(0, 0xFFFF, 0))), QStringList{"turn e2 -2"});
    }

    void allEncodersInOneReportGiveAll()
    {
        TMate2Parser parser;
        parser.parse(report(5, 5, 5));
        QCOMPARE(text(parser.parse(report(6, 6, 6))), (QStringList{"turn main -1", "turn e2 1", "turn e1 1"}));
    }

    void aKeyPressAndItsRelease()
    {
        TMate2Parser parser;
        parser.parse(report(0, 0, 0));
        QCOMPARE(text(parser.parse(report(0, 0, 0, 0x01FE))), QStringList{"down button1"});
        QCOMPARE(text(parser.parse(report(0, 0, 0, kIdle))), QStringList{"up button1"});
    }

    void twoChangesAtOnceGiveBoth()
    {
        // Unlike upstream, which gives the second only on a report that may not come.
        TMate2Parser parser;
        parser.parse(report(0, 0, 0));
        QCOMPARE(text(parser.parse(report(0, 0, 0, 0x01FC))), (QStringList{"down button1", "down button2"}));
        QCOMPARE(text(parser.parse(report(0, 0, 0, 0x01FD))), QStringList{"up button1"});
        QCOMPARE(text(parser.parse(report(0, 0, 0, 0x01FE))), (QStringList{"down button1", "up button2"}));
    }

    void eachKeyBitIsItsOwnButton()
    {
        const QStringList names = {"button1", "button2", "button3", "button4", "button5",
                                   "button6", "button9", "button10", "button11"};
        for (int bit = 0; bit < names.size(); ++bit) {
            TMate2Parser parser;
            parser.parse(report(0, 0, 0));
            const auto keys = static_cast<quint16>(kIdle & ~(1U << bit));
            QCOMPARE(text(parser.parse(report(0, 0, 0, keys))), QStringList{"down " + names[bit]});
        }
    }

    void bitsAboveTheKeysAreIgnored()
    {
        TMate2Parser parser;
        parser.parse(report(0, 0, 0));
        QVERIFY(parser.parse(report(0, 0, 0, 0xFFFF)).isEmpty());
    }

    void aPressAndATurnInOneReportGiveBoth()
    {
        TMate2Parser parser;
        parser.parse(report(5, 5, 5));
        QCOMPARE(text(parser.parse(report(5, 6, 5, 0x01DF))), (QStringList{"down button6", "turn e2 1"}));
    }

    void aWrongReportIdOrAShortReportGivesNothing()
    {
        TMate2Parser parser;
        parser.parse(report(5, 5, 5));
        QVERIFY(parser.parse(report(6, 6, 6, 0x01FE, 0x02)).isEmpty());
        QVERIFY(parser.parse(report(6, 6, 6, 0x01FE).left(8)).isEmpty());
        QVERIFY(parser.parse(QByteArray()).isEmpty());
        // They changed no state.
        QCOMPARE(text(parser.parse(report(5, 6, 5))), QStringList{"turn e2 1"});
    }

    void resetSeedsAgain()
    {
        TMate2Parser parser;
        parser.parse(report(5, 5, 5, 0x01FE));
        parser.reset();
        QVERIFY(parser.parse(report(90, 90, 90)).isEmpty());
        QCOMPARE(text(parser.parse(report(90, 91, 90))), QStringList{"turn e2 1"});
    }

    void staysMarkedUntested()
    {
        QVERIFY(!HotKeyDeviceBindings::testedOnRealDevice(DeviceId::TMate2));
    }
};

QTEST_MAIN(TestDeviceParserTMate2)
#include "tst_device_parser_tmate2.moc"
