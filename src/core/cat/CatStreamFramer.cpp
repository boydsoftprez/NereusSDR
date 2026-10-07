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
// 2026-10-06 - CAT review: CR+LF pairs removed anywhere and leading white
//              space trimmed, as TCPIPcatServer.cs:280,306 do. J.J. Boyd
//              (KG4VCF), AI-assisted via Anthropic Claude Code.

#include "CatStreamFramer.h"
#include "CatCommandCatalog.h"
namespace NereusSDR {
namespace {
// The leading white space .NET String.Trim() removes from an ASCII-decoded
// frame: TAB, LF, VT, FF, CR and space.
bool isTrimmed(char byte) { return byte == ' ' || (byte >= '\t' && byte <= '\r'); }
}
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
        // From Thetis CAT/TCPIPcatServer.cs:280 [v2.10.3.15]:
        //   m_oneLineBuf.Replace(Environment.NewLine, "");
        // so can be used a by raw telnet client
        // [original inline comment from TCPIPcatServer.cs:280]
        // Environment.NewLine is CR+LF on Windows: each pair is removed
        // wherever it falls, so a CR waits for the next byte.
        if (m_pendingCr) {
            m_pendingCr = false;
            if (byte == '\n') { continue; }
            take('\r', frames);
        }
        if (byte == '\r') { m_pendingCr = true; continue; }
        take(byte, frames);
    }
    return frames;
}
void CatStreamFramer::take(char byte, QList<QByteArray>& frames)
{
    if (m_discarding) {
        if (byte == ';') { m_discarding = false; }
        return;
    }
    // From Thetis CAT/TCPIPcatServer.cs:306 [v2.10.3.15]:
    //   sInboundCatCommand = sInboundCatCommand.Trim();
    // A frame ends at ';', so only its leading white space is trimmed;
    // payload spaces and case stay intact.
    if (m_buffer.isEmpty() && isTrimmed(byte)) { return; }
    if (byte != ';' && m_buffer.size() >= m_maximumRequestBytes - 1) {
        // not likely to be a cat message if it got this long
        // [original inline comment from TCPIPcatServer.cs:295]
        // Catalogue limit includes ';': one error then discard to boundary.
        m_buffer.clear(); m_discarding = true; frames.append(QByteArray());
        return;
    }
    m_buffer.append(byte);
    if (byte == ';') { frames.append(m_buffer); m_buffer.clear(); }
}
void CatStreamFramer::reset() { m_buffer.clear(); m_discarding = false; m_pendingCr = false; }
} // namespace NereusSDR
