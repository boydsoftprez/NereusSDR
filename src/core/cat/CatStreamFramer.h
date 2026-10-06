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

#pragma once
#include <QByteArray>
#include <QList>
namespace NereusSDR {
class CatStreamFramer {
public:
    explicit CatStreamFramer(qsizetype maximumRequestBytes = 0);
    // Empty entries are oversize error events, never parsed requests.
    QList<QByteArray> feed(const QByteArray&);
    void reset();
    qsizetype bufferedBytes() const { return m_buffer.size(); }
private:
    qsizetype m_maximumRequestBytes;
    QByteArray m_buffer;
    bool m_discarding{false};
};
} // namespace NereusSDR
