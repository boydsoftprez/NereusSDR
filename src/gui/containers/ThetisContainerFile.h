#pragma once
// NereusSDR-original Thetis .dat container envelope.
//
// One Thetis container per file:
//   Base64( gzip( BinaryFormatter( Dictionary<string,string> ) ) )
// The .NET envelope is handled by tools/thetis-dat-bridge; this class owns
// the semantic mapping between the Thetis keys and a NereusSDR document.
#include "ContainerDocument.h"
#include <QByteArray>
#include <QString>

namespace NereusSDR {

class ThetisContainerFile {
public:
    // True when the .NET bridge executable is next to this process (or named
    // by NEREUS_THETIS_DAT_BRIDGE).
    static bool available();
    static QString bridgePath();

    // A Thetis .dat file's text (Base64 envelope). Empty with *error set when
    // the bridge is unavailable or either side refuses the payload.
    static QByteArray encode(const ContainerDocument& container, QString* error = nullptr);

    // Decode a Thetis .dat file. A NereusSDR item payload stored in the same
    // dictionary (ContainerItems_<id>) is used when present, so a container
    // saved here round-trips exactly. A pure Thetis file falls back to its
    // ItemGroupData_<guid> records: types and positions are mapped, appearance
    // settings are retained raw for a later decoder.
    static DocumentResult decode(const QByteArray& bytes);
};

} // namespace NereusSDR
