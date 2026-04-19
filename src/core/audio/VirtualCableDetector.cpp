// src/core/audio/VirtualCableDetector.cpp (NereusSDR)
// Implements VirtualCableDetector: regex product matching + PortAudio scan.
// NereusSDR-original; no Thetis/AetherSDR port.
#include "VirtualCableDetector.h"
#include <QRegularExpression>
#include "PortAudioBus.h"

using namespace NereusSDR;

VirtualCableProduct VirtualCableDetector::matchProduct(const QString& n) {
    // Rule order is load-bearing:
    //   - NereusSdrVax first (reserved prefix must not be shadowed by CABLE rules)
    //   - CABLE-A/B/C/D before CABLE (or the base rule shadows the variants)
    //   - VbHiFiCable before CABLE (name contains "Hi-Fi Cable", not "CABLE ")
    // Rules are function-local statics — QRegularExpression constructed once
    // on first call (thread-safe per C++11).
    static const struct { QRegularExpression re; VirtualCableProduct p; } rules[] = {
        { QRegularExpression("^NereusSDR VAX \\d$"),                              VirtualCableProduct::NereusSdrVax },
        { QRegularExpression("^CABLE-A ",  QRegularExpression::CaseInsensitiveOption), VirtualCableProduct::VbCableA },
        { QRegularExpression("^CABLE-B ",  QRegularExpression::CaseInsensitiveOption), VirtualCableProduct::VbCableB },
        { QRegularExpression("^CABLE-C ",  QRegularExpression::CaseInsensitiveOption), VirtualCableProduct::VbCableC },
        { QRegularExpression("^CABLE-D ",  QRegularExpression::CaseInsensitiveOption), VirtualCableProduct::VbCableD },
        { QRegularExpression("Hi-?Fi Cable", QRegularExpression::CaseInsensitiveOption), VirtualCableProduct::VbHiFiCable },
        { QRegularExpression("^CABLE ",    QRegularExpression::CaseInsensitiveOption), VirtualCableProduct::VbCable },
        { QRegularExpression("Virtual Audio Cable", QRegularExpression::CaseInsensitiveOption), VirtualCableProduct::MuzychenkoVac },
        { QRegularExpression("^Line \\d+", QRegularExpression::CaseInsensitiveOption), VirtualCableProduct::MuzychenkoVac },
        { QRegularExpression("VoiceMeeter", QRegularExpression::CaseInsensitiveOption), VirtualCableProduct::Voicemeeter },
        { QRegularExpression("^Dante ",    QRegularExpression::CaseInsensitiveOption), VirtualCableProduct::Dante },
        { QRegularExpression("^DAX Audio", QRegularExpression::CaseInsensitiveOption), VirtualCableProduct::FlexRadioDax },
    };
    for (const auto& r : rules) {
        if (r.re.match(n).hasMatch()) { return r.p; }
    }
    return VirtualCableProduct::None;
}

QString VirtualCableDetector::installUrl(VirtualCableProduct p) {
    switch (p) {
        case VirtualCableProduct::VbCable:
        case VirtualCableProduct::VbCableA:
        case VirtualCableProduct::VbCableB:
        case VirtualCableProduct::VbCableC:
        case VirtualCableProduct::VbCableD:
        case VirtualCableProduct::VbHiFiCable:
            return "https://vb-audio.com/Cable/";
        case VirtualCableProduct::MuzychenkoVac:
            return "https://vac.muzychenko.net/en/";
        case VirtualCableProduct::Voicemeeter:
            return "https://voicemeeter.com/";
        case VirtualCableProduct::Dante:
            return "https://www.getdante.com/products/software-essentials/dante-virtual-soundcard/";
        default:
            return {};
    }
}

QVector<DetectedCable> VirtualCableDetector::scan() {
    QVector<DetectedCable> out;
    for (const auto& api : PortAudioBus::hostApis()) {
        for (const auto& dev : PortAudioBus::outputDevicesFor(api.index)) {
            const auto p = matchProduct(dev.name);
            if (p != VirtualCableProduct::None) {
                out.push_back({p, dev.name, false, 0});
            }
        }
        for (const auto& dev : PortAudioBus::inputDevicesFor(api.index)) {
            const auto p = matchProduct(dev.name);
            if (p != VirtualCableProduct::None) {
                out.push_back({p, dev.name, true, 0});
            }
        }
    }
    return out;
}
