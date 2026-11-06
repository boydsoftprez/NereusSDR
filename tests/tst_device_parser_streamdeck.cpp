// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 14.
//                AI-assisted implementation via Anthropic Claude Code.
// no-port-check: NereusSDR-original test.
//
// Simulated Stream Deck+ reports from the documented layout (R-HK-15). A pass shows the
// code matches its sources, not what a device does.
#include <QtTest>

#include <initializer_list>

#include "core/hotkeys/HotKeyDeviceBindings.h"
#include "device_parser_test_util.h"
#include "gui/hotkeys/devices/StreamDeckPlusParser.h"

using namespace NereusSDR;
using parsertest::text;

namespace {

QByteArray keys(std::initializer_list<int> down)
{
    QByteArray bytes(14, '\0');
    bytes[0] = 0x01;
    for (const int key : down) {
        bytes[3 + key] = 0x01;
    }
    return bytes;
}

QByteArray dial(quint8 subtype, qint8 d1, qint8 d2 = 0, qint8 d3 = 0, qint8 d4 = 0)
{
    QByteArray bytes(14, '\0');
    bytes[0] = 0x01;
    bytes[1] = 0x03;
    bytes[4] = static_cast<char>(subtype);
    bytes[5] = d1;
    bytes[6] = d2;
    bytes[7] = d3;
    bytes[8] = d4;
    return bytes;
}

} // namespace

class TestDeviceParserStreamDeck : public QObject {
    Q_OBJECT

private slots:
    void aKeyDownAndUp()
    {
        StreamDeckPlusParser parser;
        QCOMPARE(text(parser.parse(keys({1}))), QStringList{"down button1"});
        QVERIFY(parser.parse(keys({1})).isEmpty());
        QCOMPARE(text(parser.parse(keys({}))), QStringList{"up button1"});
    }

    void keysTogetherGiveAllDowns()
    {
        StreamDeckPlusParser parser;
        QCOMPARE(text(parser.parse(keys({1, 4, 8}))), (QStringList{"down button1", "down button4", "down button8"}));
        QCOMPARE(text(parser.parse(keys({4}))), (QStringList{"up button1", "up button8"}));
    }

    void aDialPress()
    {
        StreamDeckPlusParser parser;
        QCOMPARE(text(parser.parse(dial(0x00, 0, 1))), QStringList{"down dial2"});
        QCOMPARE(text(parser.parse(dial(0x00, 1, 1, 0, 1))), (QStringList{"down dial1", "down dial4"}));
        QCOMPARE(text(parser.parse(dial(0x00, 0, 0))), (QStringList{"up dial1", "up dial2", "up dial4"}));
    }

    void rotationIsSigned()
    {
        StreamDeckPlusParser parser;
        QCOMPARE(text(parser.parse(dial(0x01, 3))), QStringList{"turn dial1 3"});
        QCOMPARE(text(parser.parse(dial(0x01, -3))), QStringList{"turn dial1 -3"});
        QCOMPARE(text(parser.parse(dial(0x01, 0, 0, 0, -128))), QStringList{"turn dial4 -128"});
    }

    void allFourDialsTurningInOneReportAreReported()
    {
        StreamDeckPlusParser parser;
        QCOMPARE(text(parser.parse(dial(0x01, 1, -2, 3, -4))),
                 (QStringList{"turn dial1 1", "turn dial2 -2", "turn dial3 3", "turn dial4 -4"}));
    }

    void aTurnDoesNotDisturbAPress()
    {
        StreamDeckPlusParser parser;
        parser.parse(dial(0x00, 1));
        QCOMPARE(text(parser.parse(dial(0x01, 2))), QStringList{"turn dial1 2"});
        QVERIFY(parser.parse(dial(0x00, 1)).isEmpty());
    }

    void touchIsIgnored()
    {
        StreamDeckPlusParser parser;
        QByteArray touch = keys({1, 2});
        touch[1] = 0x02;
        QVERIFY(parser.parse(touch).isEmpty());
        // It changed no key state.
        QCOMPARE(text(parser.parse(keys({1}))), QStringList{"down button1"});
    }

    void anUnknownSubtypeOrTypeGivesNothing()
    {
        StreamDeckPlusParser parser;
        QVERIFY(parser.parse(dial(0x02, 1, 1, 1, 1)).isEmpty());
        QByteArray other = keys({1});
        other[1] = 0x04;
        QVERIFY(parser.parse(other).isEmpty());
    }

    void aWrongReportIdOrAShortReportGivesNothing()
    {
        StreamDeckPlusParser parser;
        QByteArray wrongId = keys({1});
        wrongId[0] = 0x02;
        QVERIFY(parser.parse(wrongId).isEmpty());
        QVERIFY(parser.parse(keys({1}).left(11)).isEmpty());
        QVERIFY(parser.parse(dial(0x01, 1).left(8)).isEmpty());
        QVERIFY(parser.parse(QByteArray()).isEmpty());
    }

    void resetForgetsWhatWasHeld()
    {
        StreamDeckPlusParser parser;
        parser.parse(keys({2}));
        parser.parse(dial(0x00, 1));
        parser.reset();
        QVERIFY(parser.parse(keys({})).isEmpty());
        QVERIFY(parser.parse(dial(0x00, 0)).isEmpty());
    }

    void staysMarkedUntested()
    {
        QVERIFY(!HotKeyDeviceBindings::testedOnRealDevice(DeviceId::StreamDeckPlus));
    }
};

QTEST_MAIN(TestDeviceParserStreamDeck)
#include "tst_device_parser_streamdeck.moc"
