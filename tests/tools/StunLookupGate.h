#pragma once

// no-port-check: NereusSDR-original.
// =================================================================
// tests/tools/StunLookupGate.h  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 27 (R-IOS-16), the follow-up to its re-review (new
// Minor 2): the traversal harness's station (nereus_rendezvous_peer.cpp)
// resolves the STUN names the service's hello lists, so an introduction
// chooses its STUN server by this end's address families
// (IceConfiguration). RendezvousClient registers the moment the hello
// arrives, so an introduction can arrive while that lookup is still
// running. This gate holds such an introduction until the names are
// resolved and then hands it on with them, so no introduction ever chooses
// from families not yet known.
//
// Nothing for an introduction arrives before this end answers it (the
// client gathers only once it has the answer, and the relay credentials
// follow the answer), so holding one loses nothing.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-26: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
// =================================================================

#include "core/session/IceConfiguration.h"
#include "core/session/RendezvousClient.h"

#include <QList>

#include <functional>
#include <utility>

namespace NereusSDR::Test {

class StunLookupGate {
public:
    using Handler =
        std::function<void(const RendezvousIntroduction& introduction, const HostFamilies& stun)>;

    explicit StunLookupGate(Handler handler) : m_handler(std::move(handler)) {}

    /// The service's hello arrived (RendezvousClient::connected): its STUN
    /// names are about to be resolved. Returns the lookup's number, for
    /// lookupFinished(). Introductions held from an earlier connection are
    /// dropped: they cannot be answered on this one.
    quint64 lookupStarted()
    {
        ++m_lookup;
        m_resolved = false;
        m_families.clear();
        m_waiting.clear();
        return m_lookup;
    }

    /// The names of lookup `lookup` resolved: every introduction held
    /// meanwhile is handed on, in arrival order. A lookup overtaken by a
    /// newer connection's is ignored.
    void lookupFinished(quint64 lookup, const HostFamilies& families)
    {
        if (lookup != m_lookup || m_resolved) {
            return;
        }
        m_resolved = true;
        m_families = families;
        const QList<RendezvousIntroduction> waiting = std::exchange(m_waiting, {});
        for (const RendezvousIntroduction& introduction : waiting) {
            m_handler(introduction, m_families);
        }
    }

    /// An introduction (RendezvousClient::introduced): handed on now when
    /// the names are resolved, held until they are otherwise.
    void introduce(const RendezvousIntroduction& introduction)
    {
        if (!m_resolved) {
            m_waiting.append(introduction);
            return;
        }
        m_handler(introduction, m_families);
    }

    qsizetype waiting() const { return m_waiting.size(); }

private:
    Handler m_handler;
    quint64 m_lookup = 0;
    bool m_resolved = false;
    HostFamilies m_families;
    QList<RendezvousIntroduction> m_waiting;
};

} // namespace NereusSDR::Test
