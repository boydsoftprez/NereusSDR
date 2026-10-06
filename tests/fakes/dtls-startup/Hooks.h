// no-port-check: NereusSDR-original, generated fixture observers only.
// Modification history: 2026-10-05, J.J. Boyd (KG4VCF), OpenAI Codex.
#pragma once
namespace rtc::impl { class PeerConnection; class DtlsTransport; }
void nereusDtlsStartupStart(rtc::impl::DtlsTransport* transport);
void nereusDtlsStartupInit(rtc::impl::PeerConnection* peer);
