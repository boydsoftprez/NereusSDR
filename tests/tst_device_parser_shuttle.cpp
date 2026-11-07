// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 13.
//                AI-assisted implementation via Anthropic Claude Code.
// no-port-check: NereusSDR-original test.
//
// Simulated Shuttle reports from the layout ShuttleParser.cpp cites (R-HK-15). A pass
// shows the code matches its sources, not what a device does.
#include <QtTest>

#include "core/hotkeys/HotKeyDeviceBindings.h"
#include "device_parser_test_util.h"
#include "gui/hotkeys/devices/ShuttleParser.h"

using namespace NereusSDR;
using parsertest::text;

namespace {

using Model = ShuttleParser::Model;

QByteArray report(quint8 jog, quint16 buttons = 0, qint8 ring = 0)
{
    QByteArray bytes(5, '\0');
    bytes[0] = static_cast<char>(ring);
    bytes[1] = static_cast<char>(jog);
    bytes[3] = static_cast<char>(buttons & 0xFF);
    bytes[4] = static_cast<char>(buttons >> 8);
    return bytes;
}

} // namespace

class TestDeviceParserShuttle : public QObject {
    Q_OBJECT

private slots:
    void theFirstReportOnlySeedsTheJog()
    {
        ShuttleParser parser(Model::Xpress);
        QVERIFY(parser.parse(report(200)).isEmpty());
        QCOMPARE(text(parser.parse(report(201))), QStringList{"turn jog 1"});
    }

    void theJogWraps()
    {
        ShuttleParser parser(Model::Xpress);
        parser.parse(report(255));
        QCOMPARE(text(parser.parse(report(0))), QStringList{"turn jog 1"});
        QCOMPARE(text(parser.parse(report(255))), QStringList{"turn jog -1"});
    }

    void twoButtonsInOneReportGiveTwoDowns()
    {
        ShuttleParser parser(Model::Xpress);
        parser.parse(report(0));
        QCOMPARE(text(parser.parse(report(0, 0x0030))), (QStringList{"down button1", "down button2"}));
        QCOMPARE(text(parser.parse(report(0, 0x0020))), QStringList{"up button1"});
        QCOMPARE(text(parser.parse(report(0, 0x0000))), QStringList{"up button2"});
    }

    void theXpressFiveButtonsAreBitsFourToEight()
    {
        ShuttleParser parser(Model::Xpress);
        parser.parse(report(0));
        QCOMPARE(text(parser.parse(report(0, 0x0010))), QStringList{"down button1"});
        QCOMPARE(text(parser.parse(report(0, 0x0090))), QStringList{"down button4"});
        // Byte 4, which upstream never reads.
        QCOMPARE(text(parser.parse(report(0, 0x0190))), QStringList{"down button5"});
    }

    void theXpressIgnoresThePro2Bits()
    {
        ShuttleParser parser(Model::Xpress);
        parser.parse(report(0));
        QVERIFY(parser.parse(report(0, 0x000F)).isEmpty());
        QVERIFY(parser.parse(report(0, 0x7E0F)).isEmpty());
    }

    void thePro2FifteenButtonsAreBitsZeroToFourteen()
    {
        ShuttleParser parser(Model::Pro2);
        parser.parse(report(0));
        QCOMPARE(text(parser.parse(report(0, 0x0001))), QStringList{"down button1"});
        QCOMPARE(text(parser.parse(report(0, 0x0101))), QStringList{"down button9"});
        QCOMPARE(text(parser.parse(report(0, 0x4101))), QStringList{"down button15"});
        QCOMPARE(text(parser.parse(report(0, 0x0101))), QStringList{"up button15"});
    }

    void aByteTwoValueIsNotAButton()
    {
        // Upstream reads it. The sources this follows leave it blank.
        ShuttleParser parser(Model::Pro2);
        parser.parse(report(0));
        QByteArray bytes = report(0);
        bytes[2] = 0x01;
        QVERIFY(parser.parse(bytes).isEmpty());
    }

    void button15OnThePro2ShipsUnbound()
    {
        HotKeyDeviceBindings bindings;
        QVERIFY(bindings.binding(DeviceId::ShuttlePro2, QStringLiteral("button15"), ControlGesture::Press).actionId.isEmpty());
    }

    void theRingReportsEachPosition()
    {
        ShuttleParser parser(Model::Xpress);
        parser.parse(report(0));
        QCOMPARE(text(parser.parse(report(0, 0, 3))), QStringList{"at ring 3"});
        QVERIFY(parser.parse(report(0, 0, 3)).isEmpty());
        QCOMPARE(text(parser.parse(report(0, 0, 0))), QStringList{"at ring 0"});
    }

    void theRingIsSignedAndLimited()
    {
        ShuttleParser parser(Model::Pro2);
        parser.parse(report(0));
        QCOMPARE(text(parser.parse(report(0, 0, -7))), QStringList{"at ring -7"});
        QCOMPARE(text(parser.parse(report(0, 0, 7))), QStringList{"at ring 7"});
        QVERIFY(parser.parse(report(0, 0, 8)).isEmpty());
        QVERIFY(parser.parse(report(0, 0, -8)).isEmpty());
    }

    void everyChangeInOneReportIsGiven()
    {
        ShuttleParser parser(Model::Xpress);
        parser.parse(report(10));
        QCOMPARE(text(parser.parse(report(12, 0x0010, 2))), (QStringList{"down button1", "turn jog 2", "at ring 2"}));
    }

    void aShortReportGivesNothing()
    {
        ShuttleParser parser(Model::Pro2);
        parser.parse(report(0));
        QVERIFY(parser.parse(report(5, 0x0001).left(4)).isEmpty());
    }

    void resetSeedsTheJogAgain()
    {
        ShuttleParser parser(Model::Xpress);
        parser.parse(report(10, 0x0010));
        parser.reset();
        QCOMPARE(text(parser.parse(report(99))), QStringList{});
    }

    void aFirstReportWithAButtonDownStartsNoPress()
    {
        ShuttleParser parser(Model::Pro2);
        QVERIFY(parser.parse(report(0, 0x0001)).isEmpty());
        parser.parse(report(0));
        QCOMPARE(text(parser.parse(report(0, 0x0001))), QStringList{"down button1"});
    }

    void resetArmsTheFirstReportAgain()
    {
        ShuttleParser parser(Model::Pro2);
        parser.parse(report(0));
        parser.reset();
        QVERIFY(parser.parse(report(0, 0x0001)).isEmpty());
    }

    void staysMarkedUntested()
    {
        QVERIFY(!HotKeyDeviceBindings::testedOnRealDevice(DeviceId::ShuttleXpress));
        QVERIFY(!HotKeyDeviceBindings::testedOnRealDevice(DeviceId::ShuttlePro2));
    }
};

QTEST_GUILESS_MAIN(TestDeviceParserShuttle)
#include "tst_device_parser_shuttle.moc"
