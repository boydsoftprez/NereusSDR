// no-port-check: NereusSDR-original fixture access; never in shipping RTC.
// Modification history: 2026-10-05, J.J. Boyd (KG4VCF), OpenAI Codex.
#pragma once
#include "impl/peerconnection.hpp"
#include "impl/icetransport.hpp"
#include "impl/dtlstransport.hpp"
#include "impl/processor.hpp"
#include <mutex>
#if USE_GNUTLS || USE_MBEDTLS || USE_NICE
#error This regression requires the observed OpenSSL and libjuice backend.
#endif
extern "C" void nereus_dtls_startup_connected(juice_agent_t*, int);
namespace rtc::impl {
struct NereusDtlsStartupTestAccess {
    static juice_agent_t* agent(const std::shared_ptr<IceTransport>& ice)
    { return ice->mAgent.get(); }
    static std::mutex& sslMutex(DtlsTransport* dtls) { return dtls->mSslMutex; }
    static void replaceIce(const std::shared_ptr<PeerConnection>& peer,
                           const std::shared_ptr<IceTransport>& replacement)
    { std::atomic_store(&peer->mIceTransport, replacement); }
    static void enqueue(const std::shared_ptr<PeerConnection>& peer, std::function<void()> task)
    { peer->mProcessor.enqueue(std::move(task)); }
};
}
