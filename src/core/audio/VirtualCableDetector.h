// src/core/audio/VirtualCableDetector.h (NereusSDR)
// Detects known Windows virtual-audio-cable products by regex-matching OS
// device name strings. NereusSDR-original; no Thetis/AetherSDR port.
#pragma once
#include <QString>
#include <QVector>

namespace NereusSDR {

enum class VirtualCableProduct {
    None = 0,
    VbCable,
    VbCableA,  VbCableB,  VbCableC,  VbCableD,
    VbHiFiCable,
    MuzychenkoVac,
    Voicemeeter,
    Dante,
    FlexRadioDax,
    NereusSdrVax,  // reserved for future NereusSDR-owned Windows driver
};

struct DetectedCable {
    VirtualCableProduct product;
    QString deviceName;
    bool isInput;  // false = render / output
    int channel;   // 1..N for multi-cable families; 0 if N/A
};

class VirtualCableDetector {
public:
    // Pure-function test-hook. Inspects the OS device name string.
    static VirtualCableProduct matchProduct(const QString& deviceName);

    // Enumerates OS audio devices via PortAudio and returns matches.
    static QVector<DetectedCable> scan();

    // Vendor install URL for a given product (for the first-run helper).
    static QString installUrl(VirtualCableProduct p);
};

} // namespace NereusSDR
