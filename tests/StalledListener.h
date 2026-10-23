#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// tests/StalledListener.h  (NereusSDR)
// =================================================================
//
// A silent path on this computer, for the Happy Eyeballs tests: an address
// whose connection attempts get no answer at all, so the connecting side
// waits in SYN_SENT, as it does towards an IPv6 address whose path is dead
// (2026-10-22: a Mac whose only global IPv6 address was deprecated).
// Loopback only; nothing leaves this computer. Three ways, the first that
// stalls is used:
//   - Linux: a listener on ::1 whose accept queue is full; the kernel drops
//     a new connection's SYN.
//   - Linux without IPv6 (a CI container): the same on 127.0.0.2, which
//     Linux owns along with all of 127/8.
//   - macOS: 127.0.0.2. Only 127.0.0.1 is configured on lo0, but all of
//     127/8 routes to lo0, so a SYN to 127.0.0.2 is looped back and dropped
//     unanswered. (macOS answers a full accept queue with a reset or a
//     connection, never silence, and cannot listen on 127.0.0.2.)
// Windows answers a full queue, or a port nobody listens on, with a reset
// and then retries the connect for a second or two, so a probe looks
// stalled for a while and then fails. start() therefore confirms the stall
// for kStallConfirmMs (longer than any test here waits on a silent address)
// before it returns the address; on a system with no silent path it
// returns a null address and says why in whyNot, and the test skips.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-22: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-10-22: the stall confirmed for kStallConfirmMs (Windows retries a
//               refused connect); a full queue on 127.0.0.2 for Linux
//               without IPv6. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
// =================================================================

#include <QHostAddress>
#include <QNetworkProxy>
#include <QString>
#include <QStringList>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>

#include <memory>
#include <vector>

namespace NereusSDR::Test {

class StalledListener {
public:
    /// How long a probe must stay unanswered before its address is used:
    /// past tst_happy_eyeballs' kWellInsideMs (3000 ms) and the 3000 ms the
    /// client-path tests wait for the race, so an address that only looks
    /// silent for a while (Windows' connect retries) is never used.
    static constexpr int kStallConfirmMs = 3500;

    StalledListener() = default;
    StalledListener(const StalledListener&) = delete;
    StalledListener& operator=(const StalledListener&) = delete;

    /// An address that leaves a connection to `port` unanswered, or a null
    /// address (see whyNot).
    QHostAddress start(quint16 port)
    {
        QStringList reasons;
        const QHostAddress v6(QStringLiteral("::1"));
        const QHostAddress v4(QStringLiteral("127.0.0.2"));
        for (const QHostAddress& address : {v6, v4}) {
            if (fillQueue(address, port) && confirmed()) {
                whyNot.clear();
                return address;
            }
            reasons.append(whyNot);
            reset();
        }
        if (stalls(v4, port) && confirmed()) {
            whyNot.clear();
            return v4;
        }
        reasons.append(whyNot);
        reset();
        whyNot = QStringLiteral("no silent loopback path: %1").arg(reasons.join(QStringLiteral("; ")));
        return {};
    }

    QString whyNot;

private:
    static constexpr int kStallProbeMs = 300;

    // The last probe, still opening after kStallConfirmMs in all.
    bool confirmed()
    {
        QTest::qWait(kStallConfirmMs - kStallProbeMs);
        if (!m_fillers.empty() && m_fillers.back()->state() == QAbstractSocket::ConnectingState) {
            return true;
        }
        whyNot = QStringLiteral("%1 looked silent, then answered within %2 ms")
                     .arg(m_tried.toString())
                     .arg(kStallConfirmMs);
        return false;
    }

    void reset()
    {
        m_fillers.clear();
        m_server.close();
    }

    bool stalls(const QHostAddress& address, quint16 port)
    {
        m_tried = address;
        auto probe = std::make_unique<QTcpSocket>();
        probe->setProxy(QNetworkProxy(QNetworkProxy::NoProxy));
        probe->connectToHost(address, port);
        QTest::qWait(kStallProbeMs);
        const bool stalled = probe->state() == QAbstractSocket::ConnectingState;
        m_fillers.push_back(std::move(probe));
        if (!stalled) {
            whyNot = QStringLiteral("%1 answers").arg(address.toString());
        }
        return stalled;
    }

    bool fillQueue(const QHostAddress& address, quint16 port)
    {
        m_tried = address;
        m_server.setListenBacklogSize(1);
        if (!m_server.listen(address, port)) {
            whyNot = QStringLiteral("cannot listen on %1 port %2: %3")
                         .arg(address.toString())
                         .arg(port)
                         .arg(m_server.errorString());
            return false;
        }
        m_server.pauseAccepting();
        constexpr int kMaxFillers = 64;
        for (int i = 0; i < kMaxFillers; ++i) {
            auto filler = std::make_unique<QTcpSocket>();
            filler->setProxy(QNetworkProxy(QNetworkProxy::NoProxy));
            filler->connectToHost(address, port);
            QTest::qWait(kStallProbeMs);
            const QAbstractSocket::SocketState state = filler->state();
            m_fillers.push_back(std::move(filler));
            if (state == QAbstractSocket::ConnectingState) {
                return true;
            }
            if (state != QAbstractSocket::ConnectedState) {
                whyNot = QStringLiteral("%1 refuses a connection to a full accept queue "
                                        "instead of leaving it unanswered")
                             .arg(address.toString());
                return false;
            }
        }
        whyNot = QStringLiteral("the accept queue on %1 never filled").arg(address.toString());
        return false;
    }

    QTcpServer m_server;
    QHostAddress m_tried;
    std::vector<std::unique_ptr<QTcpSocket>> m_fillers;
};

} // namespace NereusSDR::Test
