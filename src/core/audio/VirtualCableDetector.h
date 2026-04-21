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

    // Test-friendly pure filter — drops NereusSdrVax entries from an
    // arbitrary DetectedCable vector. Used by scanThirdPartyOnly() and
    // exposed as a seam so unit tests can exercise the filter without
    // standing up a PortAudio enumeration. Exposed as a public static
    // helper (not gated behind NEREUS_BUILD_TESTS like AudioEngine::*ForTest
    // seams) because it has no hidden invariants: it's a pure function on
    // its input with no side effects, so external callers can't violate
    // any class state by using it.
    static QVector<DetectedCable> filterThirdParty(const QVector<DetectedCable>& all);

    // Same as scan() but drops NereusSdrVax entries. Windows first-run path
    // calls this to surface only user-installable 3rd-party cables; the
    // NereusSdrVax enum is reserved for a future NereusSDR-owned driver and
    // must not be offered to the user as a binding target.
    static QVector<DetectedCable> scanThirdPartyOnly();

    // Vendor install URL for a given product (for the first-run helper).
    static QString installUrl(VirtualCableProduct p);

    // Startup rescan-diff helpers.
    //
    // fingerprintCsv() hashes the device-name string of each detected cable
    // to an 8-hex-char tag (SHA-256 truncated) and comma-joins the tags.
    // Empty string for an empty input; no trailing comma. SHA-256 is chosen
    // over qHash so the tag is stable across processes — qHash is randomized
    // per run and would spuriously flag every cable as "new" on every launch.
    // 8 hex chars = 32 bits of key space. At the expected population
    // (<50 audio device names on any realistic system), the birthday-
    // paradox collision probability is under 5e-7 — well below the
    // noise floor of the UX affordance this feature gates ("suggest
    // binding new cables" dialog). If we ever need to scale this to
    // thousands of entries, bump to 12-16 hex chars.
    //
    // diffNewCables() returns the cables in `current` whose fingerprint is
    // NOT present in `lastCsv`. Used by MainWindow::checkVaxFirstRun() to
    // decide whether to pop the RescanNewCables scenario on startup.
    static QString fingerprintCsv(const QVector<DetectedCable>& cables);
    static QVector<DetectedCable> diffNewCables(const QVector<DetectedCable>& current,
                                                const QString& lastCsv);
};

} // namespace NereusSDR
