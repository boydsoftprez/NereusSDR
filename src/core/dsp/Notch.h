// no-port-check: NereusSDR-original struct; the doc comment below names Thetis
// source files only to explain the field-set and default-value provenance, not
// to port code.
#pragma once

namespace NereusSDR {

/// One manual notch, in absolute RF Hz.
///
/// Handed between NotchModel (the owner), RxChannel (the WDSP fan-out) and
/// SpectrumWidget (the marker layer). It lives in core/ rather than models/ so
/// RxChannel can take it by const reference without core acquiring a dependency
/// on models; same placement rationale as dsp/ChannelConfig.h.
///
/// **Field-set provenance.** centerHz / widthHz / active correspond to the three
/// fields of Thetis's MNotch class in radio.cs (FCenter / FWidth / Active). None
/// of MNotch's logic is carried over: its Parse / ToString round-trip exists to
/// fit Thetis's key-value database and NereusSDR persists flat AppSettings keys
/// instead, and its CompareTo has no NereusSDR caller. `id` has no Thetis
/// counterpart at all; Thetis identifies a notch by its position in MNotchDB,
/// which is why every Thetis mutation loses the operator's selection and has to
/// recover it by searching for matching field values.
///
/// **Default width.** 200 Hz is the width Thetis gives a notch created from the
/// panadapter. The authoritative named constant lives on NotchModel; the
/// initialiser here mirrors it so a default-constructed Notch is usable.
///
/// Design: docs/architecture/2026-07-28-tunable-notch-filter-design.md
/// section 5.1.
struct Notch {
    int    id{0};           ///< stable, monotonic; UI hit-test and drag key
    double centerHz{0.0};   ///< absolute RF Hz
    double widthHz{200.0};  ///< Hz
    bool   active{true};    ///< per-notch bypass
};

}  // namespace NereusSDR
