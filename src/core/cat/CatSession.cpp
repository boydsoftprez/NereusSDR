// no-port-check: NereusSDR-original CAT station admission/lifecycle.
// 2026-10-04 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#include "CatSession.h"
namespace NereusSDR {
CatSession::CatSession(quint64 id, int channel, CatTransportKind transport, const CatBinding& binding)
    : m_context{id, channel}, m_binding(binding), m_transport(transport)
{
    m_context.transmitAllowed = transport != CatTransportKind::Tester;
}
} // namespace NereusSDR
