//=================================================================
// MW0LGE 2022
//=================================================================

// inspiration from https://www.codeproject.com/Articles/5733/A-TCP-IP-Server-written-in-C
//

// Ported from Thetis Project Files/Source/Console/CAT/TCPIPcatServer.cs
// Upstream source has an author/inspiration notice; project-level GNU General Public License applies.
// Modification history (NereusSDR):
// 2026-10-04 - Native event-loop CAT adaptation by J.J. Boyd (KG4VCF),
//              AI-assisted via OpenAI Codex.

#include "CatStreamFramer.h"
#include "CatCommandCatalog.h"
namespace NereusSDR {
CatStreamFramer::CatStreamFramer(qsizetype maximumRequestBytes)
    : m_maximumRequestBytes(maximumRequestBytes > 0 ? maximumRequestBytes : CatCommandCatalog().maximumRequestBytes())
{
}
QList<QByteArray> CatStreamFramer::feed(const QByteArray& bytes)
{
    // From Thetis CAT/TCPIPcatServer.cs:275-298 [v2.10.3.15].
    //[2.10.3.9]MW0LGE fixed to handle multiple messages ending in ;
    // [original inline comment from TCPIPcatServer.cs:277]
    QList<QByteArray> frames;
    for (char byte : bytes) {
        if (m_discarding) {
            if (byte == ';') { m_discarding = false; }
            continue;
        }
        // so can be used a by raw telnet client
        // [original inline comment from TCPIPcatServer.cs:280]
        // Adaptation: CR/LF only between frames; payload spaces/case stay intact.
        if (m_buffer.isEmpty() && (byte == '\r' || byte == '\n')) { continue; }
        if (byte != ';' && m_buffer.size() >= m_maximumRequestBytes - 1) {
            // not likely to be a cat message if it got this long
            // [original inline comment from TCPIPcatServer.cs:295]
            // Catalogue limit includes ';': one error then discard to boundary.
            m_buffer.clear(); m_discarding = true; frames.append(QByteArray());
            continue;
        }
        m_buffer.append(byte);
        if (byte == ';') { frames.append(m_buffer); m_buffer.clear(); }
    }
    return frames;
}
void CatStreamFramer::reset() { m_buffer.clear(); m_discarding = false; }
} // namespace NereusSDR
