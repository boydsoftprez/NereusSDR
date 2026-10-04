// no-port-check: NereusSDR-original CAT station admission/lifecycle.
// 2026-10-04 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#pragma once
#include "CatConfiguration.h"
namespace NereusSDR {
class CatSession {
public:
    CatSession(quint64 id, int channel, CatTransportKind transport, const CatBinding& binding);
    CatSessionContext& context() { return m_context; }
    const CatSessionContext& context() const { return m_context; }
    const CatBinding& binding() const { return m_binding; }
    CatTransportKind transport() const { return m_transport; }
private:
    CatSessionContext m_context;
    CatBinding m_binding;
    CatTransportKind m_transport;
};
} // namespace NereusSDR
