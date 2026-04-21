// src/core/audio/VirtualCableDetector.cpp (NereusSDR)
// Implements VirtualCableDetector: regex product matching + PortAudio scan.
// NereusSDR-original; no Thetis/AetherSDR port.
#include "VirtualCableDetector.h"
#include <QCryptographicHash>
#include <QLatin1Char>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>
#include "PortAudioBus.h"

using namespace NereusSDR;

namespace {

// Shared SHA-256(deviceName) → 8-hex-char truncation used by both
// fingerprintCsv() and diffNewCables(). Centralised so the two helpers
// can't drift (e.g. one switching to MD5 and leaving the other stable
// would silently break rescan detection).
QString hashDeviceName(const QString& name)
{
    const QByteArray full = QCryptographicHash::hash(name.toUtf8(),
                                                     QCryptographicHash::Sha256);
    // First 4 bytes → 8 hex chars. Deterministic across processes.
    return QString::fromLatin1(full.left(4).toHex());
}

} // namespace

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

QVector<DetectedCable> VirtualCableDetector::filterThirdParty(
    const QVector<DetectedCable>& all)
{
    QVector<DetectedCable> out;
    out.reserve(all.size());
    for (const auto& c : all) {
        if (c.product != VirtualCableProduct::NereusSdrVax) {
            out.push_back(c);
        }
    }
    return out;
}

QVector<DetectedCable> VirtualCableDetector::scanThirdPartyOnly()
{
    return filterThirdParty(scan());
}

QString VirtualCableDetector::fingerprintCsv(const QVector<DetectedCable>& cables)
{
    QStringList hashes;
    hashes.reserve(cables.size());
    for (const auto& c : cables) {
        hashes.append(hashDeviceName(c.deviceName));
    }
    return hashes.join(QLatin1Char(','));
}

QVector<DetectedCable> VirtualCableDetector::diffNewCables(
    const QVector<DetectedCable>& current, const QString& lastCsv)
{
    QSet<QString> known;
    const auto prev = lastCsv.split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (const auto& h : prev) {
        known.insert(h);
    }

    QVector<DetectedCable> fresh;
    for (const auto& c : current) {
        const QString h = hashDeviceName(c.deviceName);
        if (!known.contains(h)) {
            fresh.push_back(c);
        }
    }
    return fresh;
}
