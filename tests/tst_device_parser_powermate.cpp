// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 13.
//                AI-assisted implementation via Anthropic Claude Code.
// no-port-check: NereusSDR-original test.
//
// Simulated PowerMate reports and recorded Linux input events (R-HK-15). A pass shows
// the code matches its sources, not what a device does.
#include <QtTest>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "core/hotkeys/HotKeyDeviceBindings.h"
#include "device_parser_test_util.h"
#include "gui/hotkeys/devices/PowerMateLinuxInput.h"
#include "gui/hotkeys/devices/PowerMateParser.h"

using namespace NereusSDR;
using parsertest::text;

namespace {

QByteArray report(quint8 button, quint8 rotation = 0)
{
    QByteArray bytes(6, '\0');
    bytes[0] = static_cast<char>(button);
    bytes[1] = static_cast<char>(rotation);
    return bytes;
}

bool addNode(const QTemporaryDir& root, const QString& node, const QByteArray& name)
{
    const QString device = root.filePath(node + QStringLiteral("/device"));
    if (!QDir().mkpath(device)) {
        return false;
    }
    QFile file(device + QStringLiteral("/name"));
    return file.open(QIODevice::WriteOnly) && file.write(name + '\n') == name.size() + 1;
}

// From Linux include/uapi/linux/input-event-codes.h:39-41, 345, 851 [@f66469d9], as
// recorded values: a test states them on its own, or it proves nothing.
constexpr quint16 kSyn = 0;
constexpr quint16 kKey = 1;
constexpr quint16 kRel = 2;
constexpr quint16 kButton0 = 0x100;
constexpr quint16 kDial = 7;

} // namespace

class TestDeviceParserPowerMate : public QObject {
    Q_OBJECT

private slots:
    void pressAndRelease()
    {
        PowerMateParser parser;
        QVERIFY(parser.parse(report(0)).isEmpty());
        QCOMPARE(text(parser.parse(report(1))), QStringList{"down press"});
        QVERIFY(parser.parse(report(1)).isEmpty());
        QCOMPARE(text(parser.parse(report(0))), QStringList{"up press"});
    }

    void rotationIsSigned()
    {
        PowerMateParser parser;
        QCOMPARE(text(parser.parse(report(0, 0xFE))), QStringList{"turn wheel -2"});
        QCOMPARE(text(parser.parse(report(0, 0x03))), QStringList{"turn wheel 3"});
    }

    void pressAndRotateInOneReportGiveBoth()
    {
        PowerMateParser parser;
        parser.parse(report(0));
        QCOMPARE(text(parser.parse(report(1, 0x01))), (QStringList{"down press", "turn wheel 1"}));
    }

    void aShortReportGivesNothing()
    {
        PowerMateParser parser;
        QVERIFY(parser.parse(report(1).left(1)).isEmpty());
    }

    void resetForgetsThePress()
    {
        PowerMateParser parser;
        parser.parse(report(0));
        parser.parse(report(1));
        parser.reset();
        QVERIFY(parser.parse(report(0)).isEmpty());
    }

    void aFirstReportDownStartsNoPress()
    {
        PowerMateParser parser;
        QVERIFY(parser.parse(report(1)).isEmpty());
        parser.parse(report(0));
        QCOMPARE(text(parser.parse(report(1))), QStringList{"down press"});
    }

    void aTurnInTheFirstReportStillCounts()
    {
        PowerMateParser parser;
        QCOMPARE(text(parser.parse(report(1, 0x01))), QStringList{"turn wheel 1"});
    }

    void resetArmsTheFirstReportAgain()
    {
        PowerMateParser parser;
        parser.parse(report(0));
        parser.reset();
        QVERIFY(parser.parse(report(1)).isEmpty());
    }

    // ---- Linux, on recorded input_event values ----

    void linuxEventsDecode()
    {
        QCOMPARE(text(PowerMateLinuxInput::decode(kKey, kButton0, 1)), QStringList{"down press"});
        QCOMPARE(text(PowerMateLinuxInput::decode(kKey, kButton0, 0)), QStringList{"up press"});
        QCOMPARE(text(PowerMateLinuxInput::decode(kRel, kDial, 2)), QStringList{"turn wheel 2"});
        QCOMPARE(text(PowerMateLinuxInput::decode(kRel, kDial, -3)), QStringList{"turn wheel -3"});
    }

    void linuxEventsThatMeanNothingHere()
    {
        QVERIFY(PowerMateLinuxInput::decode(kSyn, 0, 0).isEmpty());
        QVERIFY(PowerMateLinuxInput::decode(kKey, kButton0, 2).isEmpty());     // a key's repeat
        QVERIFY(PowerMateLinuxInput::decode(kKey, 0x101, 1).isEmpty());
        QVERIFY(PowerMateLinuxInput::decode(kRel, 0x08, 1).isEmpty());
        QVERIFY(PowerMateLinuxInput::decode(kRel, kDial, 0).isEmpty());
    }

    void linuxNodeIsFoundByNameAlone()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(addNode(root, QStringLiteral("event0"), "AT Translated Set 2 keyboard"));
        QVERIFY(addNode(root, QStringLiteral("input5"), "Griffin PowerMate"));   // not an event node
        QCOMPARE(PowerMateLinuxInput::findEventNode(root.path()), QString());

        QVERIFY(addNode(root, QStringLiteral("event7"), "Griffin PowerMate"));
        QCOMPARE(PowerMateLinuxInput::findEventNode(root.path()), QStringLiteral("event7"));
    }

    void linuxSoundKnobMatchesToo()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(addNode(root, QStringLiteral("event2"), "Griffin SoundKnob"));
        QCOMPARE(PowerMateLinuxInput::findEventNode(root.path()), QStringLiteral("event2"));
    }

    void linuxANameThatOnlyContainsItDoesNotMatch()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        QVERIFY(addNode(root, QStringLiteral("event1"), "Griffin PowerMate Clone"));
        QCOMPARE(PowerMateLinuxInput::findEventNode(root.path()), QString());
    }

    void linuxMissingFolderFindsNothing()
    {
        QCOMPARE(PowerMateLinuxInput::findEventNode(QStringLiteral("/nonexistent/nereus")), QString());
    }

    void staysMarkedUntested()
    {
        QVERIFY(!HotKeyDeviceBindings::testedOnRealDevice(DeviceId::PowerMate));
    }
};

QTEST_GUILESS_MAIN(TestDeviceParserPowerMate)
#include "tst_device_parser_powermate.moc"
