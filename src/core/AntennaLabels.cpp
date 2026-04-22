#include "AntennaLabels.h"
#include "BoardCapabilities.h"

namespace NereusSDR {

QStringList antennaLabels(const BoardCapabilities& caps)
{
    if (!caps.hasAlex || caps.antennaInputCount < 1) {
        return {};
    }
    QStringList out;
    out.reserve(caps.antennaInputCount);
    for (int i = 1; i <= caps.antennaInputCount; ++i) {
        out << QStringLiteral("ANT%1").arg(i);
    }
    return out;
}

} // namespace NereusSDR
