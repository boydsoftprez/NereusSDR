#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// src/core/session/RendezvousMailboxTransport.h  (NereusSDR)
// =================================================================
//
// A pairing carried through the remote access service's mailbox (iPhone
// app plan Task 27, R-IOS-08; the rendezvous document,
// docs/architecture/2026-09-23-rendezvous-v1.md, section 6.5, and the link
// document's sections 3.6 and 19): the link's `pair.*` messages, each as
// the text of one mailbox body, and nothing else.
//
// It is a SessionTransport so the two ends run the pairing they already
// run on a direct connection: the Core's StationServer (admitted with
// StationServer::acceptPairingMailbox(), which skips the hellos a mailbox
// does not carry) and the desktop's StationPairingClient
// (pairByCodeOverMailbox()). Only `pair.*` messages leave: a session end or
// anything else a side would send on a direct connection is dropped here,
// because the rendezvous forwards pairing messages and nothing more.
//
// A mailbox has no address of its own and no certificate: peerAddress()
// and peerCertificateSha256() are empty, so one tap never pairs through it
// (StationServer::isOnDirectNetwork) and the Core's certificate binding is
// checked at the device's first sign-in instead (StationClient).
//
// The service holds the mailbox open for 300 s at most and ends it when
// either side's connection goes; closed() follows any of these.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-26: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
// =================================================================

#include "core/session/SessionTransport.h"

#include <QPointer>

namespace NereusSDR {

class RendezvousClient;

class RendezvousMailboxTransport final : public SessionTransport {
    Q_OBJECT

public:
    /// `client` holds an open mailbox; not owned, and may go away first.
    explicit RendezvousMailboxTransport(RendezvousClient* client, QObject* parent = nullptr);
    ~RendezvousMailboxTransport() override;

    void sendText(const QByteArray& wire) override;
    /// The service's own pings keep the mailbox's connection alive and its
    /// end is reported by mailbox.closed, so a ping here is answered at
    /// once while the mailbox is open.
    void ping() override;
    void closeLink(const QString& reason) override;
    bool isOpen() const override;
    QString peerDescription() const override;

    /// Whether `wire` is a message a mailbox carries: a JSON object whose
    /// `type` starts with "pair.".
    static bool isPairingMessage(const QByteArray& wire);

private:
    void finish();

    QPointer<RendezvousClient> m_client;
    bool m_open = true;
};

} // namespace NereusSDR
