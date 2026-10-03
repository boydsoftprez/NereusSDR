// =================================================================
// tests/tst_radio_discovery_parse.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   HPSDR/clsRadioDiscovery.cs, original licence from Thetis source is included below
//
// =================================================================
// Additional copyright holders whose code is preserved in this file via
// inline markers (upstream file-header block does not name them):
//   Reid Campbell (MI0BOT) — HermesLite 2 board-ID 6 parity test coverage
//     (preserved via inline marker on HPSDRHW::HermesLite QCOMPARE assertion)
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//   2026-09-25 - Receiver and transmit gaps plan, Task 5: the top rate a
//                 Protocol 1 reply carries. J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
// =================================================================

/*  clsRadioDiscovery.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2020-2026 Richard Samphire MW0LGE

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at

mw0lge@grange-lane.co.uk
*/
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

#include <QtTest/QtTest>
#include <QFile>
#include <QHostAddress>

#include "core/RadioDiscovery.h"
#include "core/HpsdrModel.h"

using namespace NereusSDR;

class TestRadioDiscoveryParse : public QObject {
    Q_OBJECT

private:
    // Load a fixture from tests/fixtures/discovery/<name>.
    // Comment lines (starting with #) and all whitespace are stripped
    // before QByteArray::fromHex() conversion, matching the format used
    // by the hex files created alongside this test.
    static QByteArray loadFixture(const char* name)
    {
        QFile f(QStringLiteral("%1/fixtures/discovery/%2")
                .arg(QLatin1String(TEST_DATA_DIR))
                .arg(QLatin1String(name)));
        if (!f.open(QIODevice::ReadOnly)) {
            qWarning() << "Cannot open fixture:" << f.fileName();
            return {};
        }
        const QByteArray text = f.readAll();
        QByteArray cleaned;
        for (const QByteArray& line : text.split('\n')) {
            const QByteArray trimmed = line.trimmed();
            if (trimmed.startsWith('#')) {
                continue;
            }
            cleaned += trimmed;
        }
        cleaned.replace(' ', "");
        return QByteArray::fromHex(cleaned);
    }

private slots:

    // --- P1: Hermes Lite 2 (HL2) ---
    // Fixture: fw=72, status=free, MAC=aa:bb:cc:11:22:33, board=HermesLite
    void parseP1HermesLiteReply()
    {
        const QByteArray bytes = loadFixture("p1_hermeslite_reply.hex");
        QVERIFY2(bytes.size() >= 11,
                 qPrintable(QStringLiteral("fixture size %1").arg(bytes.size())));

        RadioInfo info;
        const bool ok = RadioDiscovery::parseP1Reply(
            bytes, QHostAddress(QStringLiteral("192.168.1.42")), info);

        QVERIFY2(ok, "parseP1Reply returned false on valid HL2 reply");
        QCOMPARE(info.boardType,       HPSDRHW::HermesLite);  // MI0BOT: HL2 board-ID 6 — NereusSDR parity test [Thetis clsRadioDiscovery.cs:1239]
        QCOMPARE(info.firmwareVersion, 72);
        QCOMPARE(info.protocol,        ProtocolVersion::Protocol1);
        // MAC formatted by macToString() — upper-case hex with colons
        QCOMPARE(info.macAddress,      QStringLiteral("AA:BB:CC:11:22:33"));
        QCOMPARE(info.inUse,           false);
        // Plan Task 5: the HL2 reaches 384 kHz on Protocol 1 (mi0bot).
        QCOMPARE(info.maxSampleRate,   384000);
    }

    // --- P1: Angelia (ANAN-100D) ---
    // Wire byte 0x04 → mapP1DeviceType() → HPSDRHW::Angelia
    // (NOT the enum integer value 3 — the wire encoding differs)
    void parseP1AngeliaReply()
    {
        const QByteArray bytes = loadFixture("p1_angelia_reply.hex");
        QVERIFY2(bytes.size() >= 11,
                 qPrintable(QStringLiteral("fixture size %1").arg(bytes.size())));

        RadioInfo info;
        const bool ok = RadioDiscovery::parseP1Reply(
            bytes, QHostAddress(QStringLiteral("192.168.1.20")), info);

        QVERIFY2(ok, "parseP1Reply returned false on valid Angelia reply");
        QCOMPARE(info.boardType,       HPSDRHW::Angelia);
        QCOMPARE(info.firmwareVersion, 21);
        QCOMPARE(info.protocol,        ProtocolVersion::Protocol1);
        QCOMPARE(info.macAddress,      QStringLiteral("AA:BB:CC:44:55:66"));
        QCOMPARE(info.inUse,           false);
        // Plan Task 5: an ANAN-100D on Protocol 1 tops out at 192 kHz.
        QCOMPARE(info.maxSampleRate,   192000);
    }

    // --- P2: Saturn (ANAN-G2) ---
    // Hand-crafted fixture (no live P2 capture). Layout per clsRadioDiscovery.cs:1201-1226.
    void parseP2SaturnReply()
    {
        const QByteArray bytes = loadFixture("p2_saturn_reply.hex");
        QVERIFY2(!bytes.isEmpty(), "p2_saturn_reply.hex failed to load");
        QVERIFY2(bytes.size() >= 21,
                 qPrintable(QStringLiteral("fixture size %1").arg(bytes.size())));

        RadioInfo info;
        const bool ok = RadioDiscovery::parseP2Reply(
            bytes, QHostAddress(QStringLiteral("192.168.1.77")), info);

        QVERIFY2(ok, "parseP2Reply returned false on valid Saturn reply");
        QCOMPARE(info.boardType,       HPSDRHW::Saturn);
        QCOMPARE(info.firmwareVersion, 26);
        QCOMPARE(info.protocol,        ProtocolVersion::Protocol2);
        QCOMPARE(info.macAddress,      QStringLiteral("BB:CC:DD:77:88:99"));
        QCOMPARE(info.inUse,           false);
        // Plan Task 5 leaves a Protocol 2 reply at 1536 kHz.
        QCOMPARE(info.maxSampleRate,   1536000);
    }

    // --- Edge case: short packets rejected ---
    // Both parsers must return false on packets shorter than their minimum.
    void shortPacketRejected()
    {
        const QByteArray tooShort(5, '\0');
        RadioInfo info;

        QVERIFY2(!RadioDiscovery::parseP1Reply(
                     tooShort, QHostAddress(QStringLiteral("1.2.3.4")), info),
                 "parseP1Reply should reject 5-byte packet");
        QVERIFY2(!RadioDiscovery::parseP2Reply(
                     tooShort, QHostAddress(QStringLiteral("1.2.3.4")), info),
                 "parseP2Reply should reject 5-byte packet");
    }

    // --- in-use flag: byte[2] == 0x03 → info.inUse == true ---
    void inUseFlagParsed()
    {
        QByteArray bytes = loadFixture("p1_hermeslite_reply.hex");
        QVERIFY(bytes.size() > 2);
        bytes[2] = static_cast<char>(0x03);  // flip to in-use

        RadioInfo info;
        QVERIFY(RadioDiscovery::parseP1Reply(
            bytes, QHostAddress(QStringLiteral("192.168.1.42")), info));
        QCOMPARE(info.inUse, true);
    }

    // --- source address populated ---
    void sourceAddressPopulated()
    {
        const QByteArray bytes = loadFixture("p1_hermeslite_reply.hex");
        RadioInfo info;
        QVERIFY(RadioDiscovery::parseP1Reply(
            bytes, QHostAddress(QStringLiteral("10.0.1.5")), info));
        QCOMPARE(info.address, QHostAddress(QStringLiteral("10.0.1.5")));
    }

    // --- bad magic rejected ---
    // A P1 packet with wrong magic bytes must not parse.
    void badP1MagicRejected()
    {
        QByteArray bytes = loadFixture("p1_hermeslite_reply.hex");
        QVERIFY(bytes.size() >= 2);
        bytes[0] = static_cast<char>(0x00);  // corrupt first magic byte

        RadioInfo info;
        QVERIFY(!RadioDiscovery::parseP1Reply(
            bytes, QHostAddress(QStringLiteral("192.168.1.1")), info));
    }

    // --- Plan Task 5: the top rate a Protocol 1 reply carries ---
    // Was 384 kHz for every reply. On Protocol 1 only the RedPitaya (Thetis)
    // and the HL2 (mi0bot) reach 384 kHz, and a reply names the board, not
    // the model; the RedPitaya answers as a Hermes or OrionMKII board, so
    // its 384 kHz comes from the model the operator picks, not from here.
    void p1ReplyTopRateFollowsTheBoard_data()
    {
        QTest::addColumn<int>("boardByte");
        QTest::addColumn<int>("expected");
        QTest::newRow("Atlas")      << 0  << 192000;
        QTest::newRow("Hermes")     << 1  << 192000;
        QTest::newRow("HermesII")   << 2  << 192000;
        QTest::newRow("Angelia")    << 4  << 192000;
        QTest::newRow("Orion")      << 5  << 192000;
        QTest::newRow("HermesLite") << 6  << 384000;
        QTest::newRow("OrionMKII")  << 10 << 192000;
        QTest::newRow("HermesC10")  << 20 << 192000;
        // Plan Task 15: the HL2 receive-only kit (NereusSDR's board 12)
        // resolves to the HL2 model, so its reply carries the HL2's 384 kHz
        // (mi0bot-Thetis setup.cs:849-851 [v2.10.3.13-beta2]).
        QTest::newRow("HermesLiteRxOnly") << 12 << 384000;
    }

    void p1ReplyTopRateFollowsTheBoard()
    {
        QFETCH(int, boardByte);
        QFETCH(int, expected);
        // EF FE 02 | MAC (6) | firmware | board, padded to a full reply.
        QByteArray bytes(60, '\0');
        bytes[0] = char(0xEF);
        bytes[1] = char(0xFE);
        bytes[2] = char(0x02);
        for (int i = 3; i < 9; ++i) { bytes[i] = char(0x10 + i); }
        bytes[9]  = char(30);
        bytes[10] = char(boardByte);

        RadioInfo info;
        QVERIFY(RadioDiscovery::parseP1Reply(
            bytes, QHostAddress(QStringLiteral("192.168.1.50")), info));
        QCOMPARE(info.maxSampleRate, expected);
    }

    // Plan Task 15: the kit's receiver count is the HL2's. mi0bot sets
    // P1_rxcount = 4 for HERMESLITE (console.cs:8409-8488 [v2.10.3.13-beta2]).
    void p1HermesLiteRxOnlyReceiverCountIsTheHl2s()
    {
        auto reply = [](int boardByte) {
            QByteArray bytes(60, '\0');
            bytes[0] = char(0xEF);
            bytes[1] = char(0xFE);
            bytes[2] = char(0x02);
            for (int i = 3; i < 9; ++i) { bytes[i] = char(0x10 + i); }
            bytes[9]  = char(72);
            bytes[10] = char(boardByte);
            RadioInfo info;
            RadioDiscovery::parseP1Reply(
                bytes, QHostAddress(QStringLiteral("192.168.1.51")), info);
            return info;
        };
        const RadioInfo kit = reply(12);
        const RadioInfo hl2 = reply(6);
        QCOMPARE(kit.boardType, HPSDRHW::HermesLiteRxOnly);
        QCOMPARE(kit.maxReceivers, 4);
        QCOMPARE(kit.maxReceivers, hl2.maxReceivers);
        QCOMPARE(kit.adcCount, hl2.adcCount);
    }

    // --- P1: ANAN-G2E (HermesC10) ---
    // From Thetis ChannelMaster/network.h:420-425 [v2.10.3.15] — upstream enum context:
    //   HermesLite = 6,     // MI0BOT
    //   Saturn = 10,        // ANAN-G2: added G8NJJ
    //   HermesC10 = 20      // ANAN-G2E //N1GP G2E added (HermesC10)
    // Wire byte 0x14 (decimal 20) → HPSDRHW::HermesC10.
    void parseP1HermesC10Reply()
    {
        const QByteArray bytes = loadFixture("p1_hermesc10_reply.hex");
        QVERIFY2(bytes.size() >= 11,
                 qPrintable(QStringLiteral("fixture size %1").arg(bytes.size())));

        RadioInfo info;
        const bool ok = RadioDiscovery::parseP1Reply(
            bytes, QHostAddress(QStringLiteral("192.168.1.99")), info);

        QVERIFY2(ok, "parseP1Reply returned false on valid HermesC10 reply");
        // Upstream attribution carried from network.h:420-425 [v2.10.3.15]:
        //   HermesLite = 6  //MI0BOT  |  Saturn = 10  //G8NJJ  |  HermesC10 = 20  //N1GP G2E added
        // From Thetis network.h:425 [v2.10.3.15] //N1GP G2E added (HermesC10)
        QCOMPARE(info.boardType,       HPSDRHW::HermesC10);  // wire byte 0x14 = 20
        QCOMPARE(info.firmwareVersion, 10);
        QCOMPARE(info.protocol,        ProtocolVersion::Protocol1);
        QCOMPARE(info.macAddress,      QStringLiteral("AA:BB:CC:77:88:99"));
        QCOMPARE(info.inUse,           false);
    }
};

QTEST_APPLESS_MAIN(TestRadioDiscoveryParse)
#include "tst_radio_discovery_parse.moc"
