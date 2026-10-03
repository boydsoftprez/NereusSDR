// tests/fakes/P2FakeRadio.h
//
// no-port-check: NereusSDR-original loopback test fixture. It implements the
// documented Protocol 2 packet shapes needed to exercise the production UDP
// path; it is not a translation of an upstream implementation.

#pragma once

#include "core/RadioDiscovery.h"

#include <QHostAddress>
#include <QObject>
#include <QVector>

#include <array>

class QUdpSocket;

namespace NereusSDR::Test {

class P2FakeRadio final : public QObject {
    Q_OBJECT

public:
    explicit P2FakeRadio(QObject* parent = nullptr);
    ~P2FakeRadio() override;

    // Bind one socket for every Protocol 2 port role. preferredOutboundBase
    // is useful when two loopback addresses must represent old/new endpoints
    // with the same negotiated layout; zero selects an available base.
    bool start(const QHostAddress& address = QHostAddress(QHostAddress::LocalHost),
               quint16 preferredOutboundBase = 0);
    void stop();

    RadioInfo radioInfo() const;
    QHostAddress localAddress() const { return m_address; }
    quint16 outboundPortBase() const { return m_outboundBase; }
    quint16 inputRolePortBase() const { return m_inputRoleBase; }

    bool hasClient() const { return m_clientPort != 0; }
    QHostAddress clientAddress() const { return m_clientAddress; }
    quint16 clientPort() const { return m_clientPort; }

    void stopIngress() { m_ingressEnabled = false; }
    void resumeIngress() { m_ingressEnabled = true; }
    bool ingressEnabled() const { return m_ingressEnabled; }

    void sendDdc(int ddc = 2, float iSample = 0.5f, float qSample = 0.0f);
    // Skip `count` DDC sequence numbers, as a radio whose I/Q datagrams were
    // lost on the way would appear (R-R3-32, parity Task 6).
    void skipDdcSequence(quint32 count) { m_ddcSequence += count; }
    void sendStatus();
    void sendWideband(int adc = 0);
    void sendStatusTo(const QHostAddress& clientAddress, quint16 clientPort);
    void sendMalformedStatus();
    void sendWrongRoleStatus();
    bool sendWrongAddressStatus();

    int totalEgressDatagrams() const { return m_totalEgressDatagrams; }
    int highPriorityDatagrams() const { return m_highPriorityDatagrams; }
    int stopCount() const { return m_stopCount; }
    int moxAssertedCount() const { return m_moxAssertedCount; }
    quint8 lastHighPriorityFlags() const { return m_lastHighPriorityFlags; }
    // Byte 1401 of the last high-priority packet, the band outputs, as the
    // OC byte (network.c: (oc_output << 1) & 0xfe). -1 before one arrives.
    int lastHighPriorityOcByte() const { return m_lastHighPriorityOcByte; }
    // General command packets (60 bytes, command byte 0x00, outbound base
    // port) and byte 38 of the last one, the network watchdog (R-R3-49).
    int generalDatagrams() const { return m_generalDatagrams; }
    int lastGeneralWatchdog() const { return m_lastGeneralWatchdog; }

private:
    static constexpr int kRoleSocketCount = 18; // outbound base .. base+17

    bool bindRoleSockets(const QHostAddress& address, quint16 outboundBase);
    void clearSockets();
    void drainRoleSocket(int offset);
    void learnClient(const QHostAddress& address, quint16 port);
    void sendFromRole(int roleIndex, const QByteArray& datagram);
    QByteArray buildDdcPacket(quint32 sequence, float iSample,
                              float qSample) const;

    std::array<QUdpSocket*, kRoleSocketCount> m_roleSockets{};
    QUdpSocket* m_wrongAddressSocket{nullptr};
    QHostAddress m_address;
    quint16 m_outboundBase{0};
    quint16 m_inputRoleBase{0};
    QHostAddress m_clientAddress;
    quint16 m_clientPort{0};
    bool m_ingressEnabled{true};
    int m_totalEgressDatagrams{0};
    int m_highPriorityDatagrams{0};
    int m_stopCount{0};
    int m_moxAssertedCount{0};
    quint8 m_lastHighPriorityFlags{0};
    int m_lastHighPriorityOcByte{-1};
    int m_generalDatagrams{0};
    int m_lastGeneralWatchdog{-1};
    quint32 m_ddcSequence{0};
    quint32 m_statusSequence{0};
    std::array<quint32, 8> m_widebandSequence{};
};

} // namespace NereusSDR::Test
