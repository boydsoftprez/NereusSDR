// =================================================================
// tests/tst_radio_discovery_parse.cpp  (NereusSDR)
// =================================================================
//
// Ported from mi0bot/Thetis-HL2 source:
//   HPSDR/clsRadioDiscovery.cs
//
// Original copyright and license (preserved per GNU GPL):
//
//   Thetis / Thetis-HL2 is a C# implementation of a Software Defined Radio.
//   Copyright (C) 2004-2009  FlexRadio Systems
//   Copyright (C) 2010-2020  Doug Wigley (W5WC)
//   Copyright (C) 2019-2026  Richard Samphire (MW0LGE) — Thetis heavy modifications
//   Copyright (C) 2020-2026  Reid Campbell (MI0BOT) — Hermes-Lite fork contributions
//
//   This program is free software; you can redistribute it and/or
//   modify it under the terms of the GNU General Public License
//   as published by the Free Software Foundation; either version 2
//   of the License, or (at your option) any later version.
//
//   This program is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU General Public License for more details.
//
// Dual-Licensing Statement (applies ONLY to Richard Samphire MW0LGE's
// contributions in the upstream Thetis source — preserved verbatim from
// Thetis LICENSE-DUAL-LICENSING):
//
//   For any code originally written by Richard Samphire MW0LGE, or for
//   any modifications made by him, the copyright holder for those
//   portions (Richard Samphire) reserves the right to use, license, and
//   distribute such code under different terms, including closed-source
//   and proprietary licences, in addition to the GNU General Public
//   License granted in LICENCE. Nothing in this statement restricts any
//   rights granted to recipients under the GNU GPL.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code. Structural template follows AetherSDR
//                 (ten9876/AetherSDR) Qt6 conventions.
// =================================================================

// tst_radio_discovery_parse.cpp
//
// Unit tests for RadioDiscovery::parseP1Reply() and parseP2Reply().
// Fixtures live under tests/fixtures/discovery/ and are loaded via
// TEST_DATA_DIR, which CMakeLists.txt defines at compile time as
// ${CMAKE_CURRENT_SOURCE_DIR} (i.e. the tests/ directory).
//
// P1 byte layout verified against:
//   docs/protocols/openhpsdr-protocol1-capture-reference.md §2.2
//   mi0bot clsRadioDiscovery.cs:1145-1195 (parseDiscoveryReply P1 branch)
//
// P2 byte layout verified against:
//   mi0bot clsRadioDiscovery.cs:1201-1226 (parseDiscoveryReply P2 branch)

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
        QCOMPARE(info.boardType,       HPSDRHW::HermesLite);
        QCOMPARE(info.firmwareVersion, 72);
        QCOMPARE(info.protocol,        ProtocolVersion::Protocol1);
        // MAC formatted by macToString() — upper-case hex with colons
        QCOMPARE(info.macAddress,      QStringLiteral("AA:BB:CC:11:22:33"));
        QCOMPARE(info.inUse,           false);
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
};

QTEST_APPLESS_MAIN(TestRadioDiscoveryParse)
#include "tst_radio_discovery_parse.moc"
