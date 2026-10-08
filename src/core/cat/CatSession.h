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
#pragma once
#include "CatConfiguration.h"
#include "CatStreamFramer.h"
#include <QSet>
#include <QUuid>
#include <functional>
#include <utility>
#include <memory>
namespace NereusSDR {
class RigctlProtocol;
class CatSession {
public:
    CatSession(quint64 id, int channel, CatTransportKind transport, const CatBinding& binding, CatWireDialect dialect = CatWireDialect::Thetis);
    ~CatSession();
    CatWireDialect dialect() const { return m_dialect; }
    QList<QByteArray> feedLines(const QByteArray&);
    void setRigctlProtocol(std::shared_ptr<RigctlProtocol> protocol) { m_rigctl = std::move(protocol); }
    std::shared_ptr<RigctlProtocol> rigctlProtocol() const { return m_rigctl; }
    CatSessionContext& context() { return m_context; }
    const CatSessionContext& context() const { return m_context; }
    const CatBinding& binding() const { return m_binding; }
    CatTransportKind transport() const { return m_transport; }
    CatStreamFramer& framer() { return m_framer; }
    void applyGuidResult(const CatRequest&, const CatCommandResult&);
    bool hasGuid(const QUuid& guid) const { return m_guids.contains(guid); }
    void clearRuntime();
    void setOutputHooks(std::function<bool(const QByteArray&)> write, std::function<void()> close) { m_write = std::move(write); m_close = std::move(close); }
    bool writeBytes(const QByteArray& bytes) { return m_write && m_write(bytes); }
    void closeTransport() { if (m_close) { const auto close = std::exchange(m_close, {}); m_write = {}; close(); } }
private:
    CatSessionContext m_context;
    CatBinding m_binding;
    CatTransportKind m_transport;
    CatWireDialect m_dialect;
    QByteArray m_lineBuffer;
    bool m_droppingLine{false};
    std::shared_ptr<RigctlProtocol> m_rigctl;
    CatStreamFramer m_framer;
    QSet<QUuid> m_guids;
    std::function<bool(const QByteArray&)> m_write;
    std::function<void()> m_close;
};
} // namespace NereusSDR
