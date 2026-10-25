// =================================================================
// tests/fakes/DisplayVersionTap.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  NSDC v2 interop (display codec v2,
// Task 6): the version byte of every display codec message a window's media
// connections delivered to it, one list per media peer, so a test over real
// DTLS/SRTP (the tunnel, the web relay, a replace) can show which version
// each connection carried.
//
// Modification history (NereusSDR):
//   2026-10-22: created.  J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
// =================================================================

#pragma once

#include "core/session/media/MediaPeer.h"

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QPointer>

#include <memory>
#include <vector>

namespace NereusSDR::Test {

class DisplayVersionTap {
public:
    struct Peer {
        QPointer<MediaPeer> peer;
        QList<int> versions;
    };

    // `window`: the RemoteMediaController whose media peers are tapped. The
    // tap must not outlive it.
    explicit DisplayVersionTap(QObject* window) : m_window(window) {}
    DisplayVersionTap(const DisplayVersionTap&) = delete;
    DisplayVersionTap& operator=(const DisplayVersionTap&) = delete;

    // Taps every media peer the window made since the last call; returns
    // how many peers are tapped. Call it while waiting, so a peer is tapped
    // before its displays start.
    int follow()
    {
        const QList<MediaPeer*> peers =
            m_window->findChildren<MediaPeer*>(Qt::FindDirectChildrenOnly);
        for (MediaPeer* peer : peers) {
            bool known = false;
            for (const std::unique_ptr<Peer>& entry : m_peers) {
                if (entry->peer == peer) {
                    known = true;
                    break;
                }
            }
            if (known) {
                continue;
            }
            auto entry = std::make_unique<Peer>();
            entry->peer = peer;
            Peer* const tapped = entry.get();
            // The tap's own context: the window cuts its own connections to a
            // peer it promotes (disconnect(peer, nullptr, window, nullptr)),
            // and that must not cut the tap.
            QObject::connect(peer, &MediaPeer::displayReceived, &m_context,
                             [tapped](const QByteArray& message) {
                                 if (message.size() > 4 && message.startsWith("NSDC")) {
                                     tapped->versions.append(
                                         static_cast<quint8>(message.at(4)));
                                 }
                             });
            m_peers.push_back(std::move(entry));
        }
        return static_cast<int>(m_peers.size());
    }

    // The peers in the order they were tapped.
    const std::vector<std::unique_ptr<Peer>>& peers() const { return m_peers; }

    // Every version byte, from every peer.
    QList<int> allVersions() const
    {
        QList<int> versions;
        for (const std::unique_ptr<Peer>& entry : m_peers) {
            versions.append(entry->versions);
        }
        return versions;
    }

private:
    QObject* m_window;
    std::vector<std::unique_ptr<Peer>> m_peers;
    // Declared last, so its connections go before the entries they write.
    QObject m_context;
};

} // namespace NereusSDR::Test
