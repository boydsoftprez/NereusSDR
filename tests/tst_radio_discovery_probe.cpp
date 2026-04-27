// =================================================================
// tests/tst_radio_discovery_probe.cpp  (NereusSDR)
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
//   2026-04-27 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code. Phase 3Q-2: unicast probe path tests.
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

#include <QObject>
#include <QSignalSpy>
#include <QUdpSocket>
#include <QtTest>
#include "core/RadioDiscovery.h"

using namespace NereusSDR;

// Fake P1 radio that listens on a loopback port and replies to a probe.
// Keeps the test self-contained — no network dependencies.
class FakeP1Probe : public QObject {
    Q_OBJECT
public:
    explicit FakeP1Probe(QObject* parent = nullptr) : QObject(parent) {
        m_socket = new QUdpSocket(this);
        m_socket->bind(QHostAddress::LocalHost, 0);
        connect(m_socket, &QUdpSocket::readyRead, this, &FakeP1Probe::onReadyRead);
    }
    quint16 port() const { return m_socket->localPort(); }
private slots:
    void onReadyRead() {
        while (m_socket->hasPendingDatagrams()) {
            QByteArray buf;
            buf.resize(int(m_socket->pendingDatagramSize()));
            QHostAddress src;
            quint16 srcPort = 0;
            m_socket->readDatagram(buf.data(), buf.size(), &src, &srcPort);

            // P1 probe is 0xEF 0xFE 0x02 (3 bytes).
            if (buf.size() >= 3 && static_cast<quint8>(buf[0]) == 0xEF
                && static_cast<quint8>(buf[1]) == 0xFE
                && static_cast<quint8>(buf[2]) == 0x02) {
                QByteArray reply(63, 0);
                reply[0] = char(0xEF); reply[1] = char(0xFE); reply[2] = 0x02;
                // MAC bytes 3..8
                reply[3] = 0x00; reply[4] = 0x1C; reply[5] = char(0xC0);
                reply[6] = char(0xA2); reply[7] = 0x14; reply[8] = char(0x8B);
                reply[9] = 75;          // firmware
                reply[10] = 6;          // boardId = HL2
                m_socket->writeDatagram(reply, src, srcPort);
            }
        }
    }
private:
    QUdpSocket* m_socket;
};

class TstRadioDiscoveryProbe : public QObject {
    Q_OBJECT
private slots:
    void probeReplyFillsRadioInfo() {
        FakeP1Probe radio;
        RadioDiscovery disc;
        QSignalSpy spy(&disc, &RadioDiscovery::radioDiscovered);

        disc.probeAddress(QHostAddress::LocalHost, radio.port(), std::chrono::milliseconds(500));

        QVERIFY(spy.wait(1000));
        QCOMPARE(spy.count(), 1);
        const auto info = spy.takeFirst().at(0).value<RadioInfo>();
        QCOMPARE(info.boardType, HPSDRHW::HermesLite);
        QCOMPARE(info.firmwareVersion, 75);
        QCOMPARE(info.macAddress, QStringLiteral("00:1C:C0:A2:14:8B"));
        QCOMPARE(info.protocol, ProtocolVersion::Protocol1);
    }

    void timeoutEmitsProbeFailed() {
        RadioDiscovery disc;
        QSignalSpy spy(&disc, &RadioDiscovery::probeFailed);
        // Localhost port that nothing is listening on — guaranteed timeout.
        disc.probeAddress(QHostAddress::LocalHost, 1, std::chrono::milliseconds(150));
        QVERIFY(spy.wait(500));
        QCOMPARE(spy.count(), 1);
    }
};

QTEST_MAIN(TstRadioDiscoveryProbe)
#include "tst_radio_discovery_probe.moc"
