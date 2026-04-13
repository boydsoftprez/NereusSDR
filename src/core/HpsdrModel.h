// src/core/HpsdrModel.h
//
// Port of HPSDR radio model and board-type enums.
// Source: mi0bot/Thetis@Hermes-Lite Project Files/Source/Console/enums.cs
//         :109 (HPSDRModel) and :388 (HPSDRHW).
// Integer values preserved exactly — including the 7..9 reserved gap in
// HPSDRHW — because the Thetis wire format compares these ints.

#pragma once

#include <QMetaType>

namespace NereusSDR {

// Logical radio model — what the user says they have in Setup.
// Source: enums.cs:109
enum class HPSDRModel : int {
    FIRST        = -1,
    HPSDR        =  0,  // Atlas/Metis kit
    HERMES       =  1,
    ANAN10       =  2,
    ANAN10E      =  3,
    ANAN100      =  4,
    ANAN100B     =  5,
    ANAN100D     =  6,
    ANAN200D     =  7,
    ORIONMKII    =  8,
    ANAN7000D    =  9,
    ANAN8000D    = 10,
    ANAN_G2      = 11,  // G8NJJ contribution
    ANAN_G2_1K   = 12,  // G8NJJ contribution
    ANVELINAPRO3 = 13,
    HERMESLITE   = 14,  // MI0BOT contribution
    REDPITAYA    = 15,  // DH1KLM contribution — enum slot preserved, impl deferred
    LAST         = 16
};

// Physical board — what's actually on the wire.
// Source: enums.cs:388 + ChannelMaster/network.h:446
enum class HPSDRHW : int {
    Atlas      =   0,  // HPSDR kit (aka Metis in PowerSDR)
    Hermes     =   1,  // ANAN-10 / ANAN-100
    HermesII   =   2,  // ANAN-10E / ANAN-100B
    Angelia    =   3,  // ANAN-100D
    Orion      =   4,  // ANAN-200D
    OrionMKII  =   5,  // ANAN-7000DLE / 8000DLE / AnvelinaPro3
    HermesLite =   6,  // Hermes Lite 2
    // 7..9 reserved — DO NOT REUSE (Thetis wire format compares these ints)
    Saturn     =  10,  // ANAN-G2
    SaturnMKII =  11,  // ANAN-G2 MkII board revision
    Unknown    = 999
};

constexpr HPSDRHW boardForModel(HPSDRModel m) noexcept {
    switch (m) {
        case HPSDRModel::HPSDR:        return HPSDRHW::Atlas;
        case HPSDRModel::HERMES:       return HPSDRHW::Hermes;
        case HPSDRModel::ANAN10:       return HPSDRHW::Hermes;
        case HPSDRModel::ANAN10E:      return HPSDRHW::HermesII;
        case HPSDRModel::ANAN100:      return HPSDRHW::Hermes;
        case HPSDRModel::ANAN100B:     return HPSDRHW::HermesII;
        case HPSDRModel::ANAN100D:     return HPSDRHW::Angelia;
        case HPSDRModel::ANAN200D:     return HPSDRHW::Orion;
        case HPSDRModel::ORIONMKII:    return HPSDRHW::OrionMKII;
        case HPSDRModel::ANAN7000D:    return HPSDRHW::OrionMKII;
        case HPSDRModel::ANAN8000D:    return HPSDRHW::OrionMKII;
        case HPSDRModel::ANAN_G2:      return HPSDRHW::Saturn;
        case HPSDRModel::ANAN_G2_1K:   return HPSDRHW::Saturn;
        case HPSDRModel::ANVELINAPRO3: return HPSDRHW::OrionMKII;
        case HPSDRModel::HERMESLITE:   return HPSDRHW::HermesLite;
        case HPSDRModel::REDPITAYA:    return HPSDRHW::OrionMKII;
        case HPSDRModel::FIRST:
        case HPSDRModel::LAST:         return HPSDRHW::Unknown;
    }
    return HPSDRHW::Unknown;
}

constexpr const char* displayName(HPSDRModel m) noexcept {
    switch (m) {
        case HPSDRModel::HPSDR:        return "HPSDR (Atlas/Metis)";
        case HPSDRModel::HERMES:       return "Hermes";
        case HPSDRModel::ANAN10:       return "ANAN-10";
        case HPSDRModel::ANAN10E:      return "ANAN-10E";
        case HPSDRModel::ANAN100:      return "ANAN-100";
        case HPSDRModel::ANAN100B:     return "ANAN-100B";
        case HPSDRModel::ANAN100D:     return "ANAN-100D";
        case HPSDRModel::ANAN200D:     return "ANAN-200D";
        case HPSDRModel::ORIONMKII:    return "Orion MkII";
        case HPSDRModel::ANAN7000D:    return "ANAN-7000DLE";
        case HPSDRModel::ANAN8000D:    return "ANAN-8000DLE";
        case HPSDRModel::ANAN_G2:      return "ANAN-G2";
        case HPSDRModel::ANAN_G2_1K:   return "ANAN-G2 1K";
        case HPSDRModel::ANVELINAPRO3: return "Anvelina Pro 3";
        case HPSDRModel::HERMESLITE:   return "Hermes Lite 2";
        case HPSDRModel::REDPITAYA:    return "Red Pitaya";
        case HPSDRModel::FIRST:
        case HPSDRModel::LAST:         return "Unknown";
    }
    return "Unknown";
}

} // namespace NereusSDR

Q_DECLARE_METATYPE(NereusSDR::HPSDRModel)
Q_DECLARE_METATYPE(NereusSDR::HPSDRHW)
