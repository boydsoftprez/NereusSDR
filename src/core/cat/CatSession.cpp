//=================================================================
// MW0LGE 2022
//=================================================================

// inspiration from https://www.codeproject.com/Articles/5733/A-TCP-IP-Server-written-in-C
//

// Ported from Thetis Project Files/Source/Console/CAT/TCPIPcatServer.cs
// Upstream source has an author/inspiration notice; project-level GNU General Public License applies.
// Modification history (NereusSDR):
// 2026-10-04 - Native session GUID adaptation by J.J. Boyd (KG4VCF),
//              AI-assisted via OpenAI Codex.
// 2026-10-04 - Native separate Hamlib dialect and guarded lifecycle integration,
//              J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex; no new Thetis port.
#include "CatSession.h"
#include "RigctlProtocol.h"
namespace NereusSDR {
namespace {
// From Thetis CAT/TCPIPcatServer.cs:315-317 [v2.10.3.15]. Strict descriptor width.
constexpr qsizetype kGuidWidth = 36;
}
CatSession::CatSession(quint64 id, int channel, CatTransportKind transport, const CatBinding& binding, CatWireDialect dialect)
    : m_context{id, channel}, m_binding(binding), m_transport(transport), m_dialect(dialect)
{
    m_context.transmitAllowed = transport != CatTransportKind::Tester;
}
CatSession::~CatSession() = default;
void CatSession::clearRuntime()
{
    m_framer.reset(); m_guids.clear(); m_lineBuffer.clear(); m_droppingLine = false;
    if (m_rigctl) { m_rigctl->reset(); }
}
QList<QByteArray> CatSession::feedLines(const QByteArray& bytes)
{
    // Native bounded stream policy: recover at newline after one oversize error.
    constexpr qsizetype kMaximumLineBytes = 4096;
    QList<QByteArray> lines;
    for (char byte : bytes) {
        if (byte == '\n') {
            if (m_droppingLine) { lines.append(QByteArray()); }
            else { lines.append(m_lineBuffer + byte); }
            m_lineBuffer.clear(); m_droppingLine = false;
        } else if (!m_droppingLine) {
            if (m_lineBuffer.size() == kMaximumLineBytes) { m_lineBuffer.clear(); m_droppingLine = true; }
            else { m_lineBuffer.append(byte); }
        }
    }
    return lines;
}
void CatSession::applyGuidResult(const CatRequest& request, const CatCommandResult& result)
{
    // From Thetis CAT/TCPIPcatServer.cs:164-238,308-346 [v2.10.3.15].
    // Special case for ZZGA and ZZGR, just parse it here as relatated to TCPcat client ID management
    // The regular cat serial parser does handle this msg if it were to come in via serial, and replies with a good or bad response, but does not do anything else with that guid.
    // We take the guids and add/remove to/from this client
    // These guids can then can then be used in a directed send
    // [original inline comment from TCPIPcatServer.cs:308-311]
    if (m_dialect != CatWireDialect::Thetis || m_transport != CatTransportKind::Tcp || result.kind != CatResultKind::Wire
        || (request.code != "ZZGA" && request.code != "ZZGR") || request.suffix.size() != kGuidWidth
        || result.data != request.code + request.suffix.toLower() + ';') { return; }
    const QUuid guid(QString::fromLatin1(request.suffix));
    if (request.code == "ZZGA") { m_guids.insert(guid); } else { m_guids.remove(guid); }
}
} // namespace NereusSDR
