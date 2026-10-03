#pragma once
// tests/fakes/LoopbackStationLink.h
//
// no-port-check: NereusSDR-original test fixture.
//
// Remote-daemon R2 Task 11 -- in-process stand-in for Task 18's wss session
// transport.
//
// There is no transport yet (Task 18 owns that): no socket, no TLS, no
// framing, no read-loop thread. This class is a plain, two-ended byte pipe
// -- "client" (the remote GUI side) and "daemon" (the RadioModel /
// StateMirror / SessionCommandDispatcher side) -- so tst_session_verbs.cpp
// can exercise the SessionMessages codec plus a real
// SessionCommandDispatcher round trip without standing up anything Task 18
// has not built yet. Whatever one side sends arrives at the other side's
// received signal via a DIRECT (non-queued) call, synchronously, before
// the send call returns: there is no I/O thread here to make it anything
// else.
//
// Deliberately not IStationLink (src/core/session/IStationLink.h): that
// interface is RadioModel's own non-owning attachment point and currently
// declares no methods to implement beyond its destructor (nothing calls
// through it yet, per its own header comment). This class is a message-
// level loopback for the codec and dispatcher, a different concern, so
// inheriting an interface with nothing to override would not buy anything.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-05  J.J. Boyd / KG4VCF  Remote daemon R2 Task 11: in-process
//                                    loopback transport fake. AI-assisted
//                                    transformation via Anthropic Claude
//                                    Code.
// =================================================================

#include <QByteArray>
#include <QObject>

namespace NereusSDR::Test {

class LoopbackStationLink : public QObject {
    Q_OBJECT

public:
    explicit LoopbackStationLink(QObject* parent = nullptr);

    /// Client -> daemon. Delivered synchronously via receivedByDaemon()
    /// before this call returns.
    void sendFromClient(const QByteArray& wire);

    /// Daemon -> client. Delivered synchronously via receivedByClient()
    /// before this call returns.
    void sendFromDaemon(const QByteArray& wire);

signals:
    void receivedByDaemon(const QByteArray& wire);
    void receivedByClient(const QByteArray& wire);
};

} // namespace NereusSDR::Test
