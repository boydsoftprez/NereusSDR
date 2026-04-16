// src/core/BoardCapabilities.cpp
//
// Per-board capability entries, verified against mi0bot/Thetis-HL2:
//   clsHardwareSpecific.cs  — ADC counts (SetRxADC), model→hardware mapping
//   Setup.cs                — attenuator ranges, sample rates, HL2 specifics
//   console.cs              — DDC config, P1/P2 routing per board, PS/diversity
//   HPSDR/NetworkIO.cs      — protocol, board discovery
//   ChannelMaster/network.h — HPSDRHW enum values :446-456
//   enums.cs                — HPSDRModel / HPSDRHW integer values :388-398
//
// ADC counts sourced from clsHardwareSpecific.cs SetRxADC() calls:
//   HERMES/HERMESLITE/ANAN10/10E/100/100B → SetRxADC(1)  (single ADC)
//   ANAN100D/200D/ORIONMKII/7000D/8000D/G2/G2_1K/ANVELINAPRO3/REDPITAYA
//                                          → SetRxADC(2)  (dual ADC)
//
// Protocol sourced from clsHardwareSpecific.cs "IsProtocol2" / HasAudioAmplifier:
//   P1: Atlas, Hermes, HermesII, Angelia, Orion, HermesLite
//   P2: OrionMKII, Saturn, SaturnMKII
//
// P1 sample rates: {48k,96k,192k} standard; HermesLite also supports 384k
//   (Setup.cs:850-853 — "The HL supports 384K")
// P2 sample rates: {48k,96k,192k,384k,768k,1536k} (Setup.cs:854)
//   Only 4 slots in the array; first 4 P2 rates listed, maxSampleRate=1536000
//
// Step attenuator:
//   Standard boards: 0..31 dB step 1 (Setup.cs:16099 "Maximum = 31")
//   HL2: -28..+32 span = 60 dB step 1 (Setup.cs:16085-16086, PerformDelayedInit:1084-1088)
//
// Diversity/PureSignal: all 2-ADC boards support both (console.cs DDC config)
//   HermesII (single ADC) does NOT have diversity; it does support PureSignal
//   (console.cs:30276-30277 — HL2 ANAN10E psform.PSEnabled reference)
//
// OC outputs: Hermes and newer ANAN (non-HL2, non-Atlas) have 7 OC outputs
//   HL2 and Atlas: 0 OC outputs
//
// Alex filters/TX routing: present on all boards with an Alex accessory port
//   (Setup.cs chkAlexPresent/chkAlexAntCtrl). Not on Atlas or HermesLite.
//
// Firmware versions: ALL boards report minFirmwareVersion = 0 and
//   knownGoodFirmware = 0. Background: Thetis enforces exactly ONE per-board
//   firmware refusal in its entire connect path —
//     NetworkIO.cs:136-143 → if (DeviceType == HermesII && CodeVersion < 103)
//   No other board has a Thetis-attested firmware floor, and Thetis has no
//   "stale firmware" warning concept at all. Rather than ship per-board
//   guessed thresholds (the previous approach, marked TODO(3I-T2)), the
//   firmware refusal + stale-warning paths in P1RadioConnection were removed
//   on 2026-04-13. The minFirmwareVersion / knownGoodFirmware fields remain
//   in the struct for now as dead metadata; they are not consulted by any
//   connect path. Display format remains CodeVersion / 10.0f
//   (clsDiscoveredRadioPicker.cs:305 / setup.cs:14008): v15 → "FW v1.5".

#include "BoardCapabilities.h"

namespace NereusSDR {
namespace {

// ─── Atlas / HPSDR kit ──────────────────────────────────────────────────────
// Source: network.h:448 (Atlas=0), clsHardwareSpecific.cs Model→Hardware map
// ADC: single (clsHardwareSpecific.cs: no explicit SetRxADC for HPSDR model,
//      defaults to 1). Metis/Atlas board is Protocol 1 only.
// No Alex port on bare Atlas kit; OC outputs via Penny board (not addressable
//   the same way). Atlas max receivers = 7 (P1 hardware limit), though
//   typically 3 in practice. maxSampleRate = 192k for P1 standard.
// Firmware floor: none. Thetis NetworkIO.cs has no Atlas firmware check.
constexpr BoardCapabilities kAtlas = {
    .board            = HPSDRHW::Atlas,
    .protocol         = ProtocolVersion::Protocol1,
    .adcCount         = 1,
    .maxReceivers     = 3,
    .sampleRates      = {48000, 96000, 192000, 0},
    .maxSampleRate    = 192000,
    .attenuator       = {0, 0, 0, false},
    .preamp           = {false, false},
    .ocOutputCount    = 0,
    .hasAlexFilters   = false,
    .hasAlexTxRouting = false,
    .xvtrJackCount    = 0,
    .antennaInputCount = 1,
    .hasPureSignal    = false,
    .hasDiversityReceiver = false,
    .hasStepAttenuatorCal = false,
    .hasPaProfile     = false,
    .hasBandwidthMonitor = false,
    .hasIoBoardHl2    = false,
    .hasSidetoneGenerator = false,
    .minFirmwareVersion = 0,
    .knownGoodFirmware  = 0,
    .displayName      = "Atlas / HPSDR kit",
    .sourceCitation   = "network.h:448, enums.cs:390, clsHardwareSpecific.cs Atlas branch",
};

// ─── Hermes (ANAN-10 / ANAN-100) ────────────────────────────────────────────
// Source: network.h:449 (Hermes=1), clsHardwareSpecific.cs:87-93
// ADC: SetRxADC(1) — single ADC. Protocol 1.
// Attenuator: 0..31 dB step 1 (Setup.cs:16099 standard path).
// Alex: present on ANAN-100; optional accessory on ANAN-10.
//   Treated as available since the hardware supports it.
// OC: 7 outputs on Hermes board.
// maxReceivers: 4 for P1 with single ADC (DDC config slots 0-3).
// Firmware floor: none. NetworkIO.cs:136-143 — the only firmware refusal in
//   Thetis is HermesII-only (DeviceType == HPSDRHW.HermesII && CodeVersion < 103).
//   Plain Hermes has no equivalent guard. v15 ("FW v1.5") is a normal
//   legitimate Hermes firmware that Thetis accepts without complaint.
constexpr BoardCapabilities kHermes = {
    .board            = HPSDRHW::Hermes,
    .protocol         = ProtocolVersion::Protocol1,
    .adcCount         = 1,
    .maxReceivers     = 4,
    .sampleRates      = {48000, 96000, 192000, 0},
    .maxSampleRate    = 192000,
    .attenuator       = {0, 31, 1, true},
    .preamp           = {true, false},
    .ocOutputCount    = 7,
    .hasAlexFilters   = true,
    .hasAlexTxRouting = true,
    .xvtrJackCount    = 1,
    .antennaInputCount = 3,
    .hasPureSignal    = false,
    .hasDiversityReceiver = false,
    .hasStepAttenuatorCal = false,
    .hasPaProfile     = true,
    .hasBandwidthMonitor = false,
    .hasIoBoardHl2    = false,
    .hasSidetoneGenerator = false,
    .minFirmwareVersion = 0,   // No Thetis-attested floor for plain Hermes
    .knownGoodFirmware  = 0,   // No Thetis-attested "known good" reference
    .displayName      = "Hermes (ANAN-10/100)",
    .sourceCitation   = "network.h:449, enums.cs:391, clsHardwareSpecific.cs:87-93; "
                        "fw floor: NetworkIO.cs:136-143 (no plain-Hermes check)",
};

// ─── HermesII (ANAN-10E / ANAN-100B) ────────────────────────────────────────
// Source: network.h:450 (HermesII=2), clsHardwareSpecific.cs:108-127
// ADC: SetRxADC(1) — single ADC. Protocol 1.
// HermesII supports PureSignal (console.cs:30276-30277 ANAN10E psform.PSEnabled).
// No diversity (single ADC). Step attenuator cal: present on HermesII.
// Note: Setup.cs:16088-16099 — HermesII is explicitly excluded from the
//   "Maximum=61" branch; uses standard 0..31 range.
// TODO(3I-T2): verify maxReceivers=4 vs 7 for HermesII
constexpr BoardCapabilities kHermesII = {
    .board            = HPSDRHW::HermesII,
    .protocol         = ProtocolVersion::Protocol1,
    .adcCount         = 1,
    .maxReceivers     = 4,
    .sampleRates      = {48000, 96000, 192000, 0},
    .maxSampleRate    = 192000,
    .attenuator       = {0, 31, 1, true},
    .preamp           = {true, false},
    .ocOutputCount    = 7,
    .hasAlexFilters   = true,
    .hasAlexTxRouting = true,
    .xvtrJackCount    = 1,
    .antennaInputCount = 3,
    .hasPureSignal    = true,
    .hasDiversityReceiver = false,
    .hasStepAttenuatorCal = true,
    .hasPaProfile     = true,
    .hasBandwidthMonitor = false,
    .hasIoBoardHl2    = false,
    .hasSidetoneGenerator = false,
    .minFirmwareVersion = 0,   // floor check removed; see file header
    .knownGoodFirmware  = 0,   // Thetis NetworkIO.cs:138 still rejects <103
                               // for HermesII, but enforcement moved upstream
                               // (see P1RadioConnection.cpp 2026-04-13)
    .displayName      = "Hermes II (ANAN-10E/100B)",
    .sourceCitation   = "network.h:450, enums.cs:392, clsHardwareSpecific.cs:108-127; "
                        "fw refusal in upstream Thetis: NetworkIO.cs:136-143 (<103)",
};

// ─── Angelia (ANAN-100D) ────────────────────────────────────────────────────
// Source: network.h:451 (Angelia=3), clsHardwareSpecific.cs:129-134
// ADC: SetRxADC(2) — dual ADC. Protocol 1.
// Diversity + PureSignal: yes (2 ADCs, console.cs GetDDC P1 Angelia branch:8680)
// maxReceivers: 7 (P1 dual-ADC limit, console.cs GetDDC Angelia branch)
// TODO(3I-T2): verify firmware versions
constexpr BoardCapabilities kAngelia = {
    .board            = HPSDRHW::Angelia,
    .protocol         = ProtocolVersion::Protocol1,
    .adcCount         = 2,
    .maxReceivers     = 7,
    .sampleRates      = {48000, 96000, 192000, 384000},
    .maxSampleRate    = 384000,
    .attenuator       = {0, 31, 1, true},
    .preamp           = {true, false},
    .ocOutputCount    = 7,
    .hasAlexFilters   = true,
    .hasAlexTxRouting = true,
    .xvtrJackCount    = 1,
    .antennaInputCount = 3,
    .hasPureSignal    = true,
    .hasDiversityReceiver = true,
    .hasStepAttenuatorCal = true,
    .hasPaProfile     = true,
    .hasBandwidthMonitor = false,
    .hasIoBoardHl2    = false,
    .hasSidetoneGenerator = false,
    .minFirmwareVersion = 0,   // floor check removed; see file header
    .knownGoodFirmware  = 0,
    .displayName      = "ANAN-100D (Angelia)",
    .sourceCitation   = "network.h:451, enums.cs:393, clsHardwareSpecific.cs:129-134",
};

// ─── Orion (ANAN-200D) ──────────────────────────────────────────────────────
// Source: network.h:452 (Orion=4), clsHardwareSpecific.cs:136-141
// ADC: SetRxADC(2) — dual ADC. Protocol 1 (P1 board).
// Diversity + PureSignal: yes (2 ADCs, console.cs GetDDC P1 Orion branch:8681)
// Preamp: hasBypassAndPreamp=true (50V supply vs 33V for Hermes/Angelia,
//   clsHardwareSpecific.cs:139 SetADCSupply(0,50))
// TODO(3I-T2): verify firmware versions
constexpr BoardCapabilities kOrion = {
    .board            = HPSDRHW::Orion,
    .protocol         = ProtocolVersion::Protocol1,
    .adcCount         = 2,
    .maxReceivers     = 7,
    .sampleRates      = {48000, 96000, 192000, 384000},
    .maxSampleRate    = 384000,
    .attenuator       = {0, 31, 1, true},
    .preamp           = {true, true},
    .ocOutputCount    = 7,
    .hasAlexFilters   = true,
    .hasAlexTxRouting = true,
    .xvtrJackCount    = 1,
    .antennaInputCount = 3,
    .hasPureSignal    = true,
    .hasDiversityReceiver = true,
    .hasStepAttenuatorCal = true,
    .hasPaProfile     = true,
    .hasBandwidthMonitor = false,
    .hasIoBoardHl2    = false,
    .hasSidetoneGenerator = false,
    .minFirmwareVersion = 0,   // floor check removed; see file header
    .knownGoodFirmware  = 0,
    .displayName      = "ANAN-200D (Orion)",
    .sourceCitation   = "network.h:452, enums.cs:394, clsHardwareSpecific.cs:136-141",
};

// ─── OrionMKII (ANAN-7000DLE / 8000DLE / AnvelinaPro3 / RedPitaya) ──────────
// Source: network.h:453 (OrionMKII=5), clsHardwareSpecific.cs:143-190
// ADC: SetRxADC(2) — dual ADC. Protocol 2 (clsHardwareSpecific.cs HasAudioAmplifier
//   check: "protocol 2 only for 7000D/8000D/AnvelinaPro3/RedPitaya").
// P2 sample rates: 48k/96k/192k/384k/768k/1536k (Setup.cs:854);
//   array stores first 4, maxSampleRate = 1536000.
// Correction vs plan baseline: maxSampleRate updated to 1536000 (P2 boards)
// TODO(3I-T2): verify firmware versions for OrionMKII
constexpr BoardCapabilities kOrionMKII = {
    .board            = HPSDRHW::OrionMKII,
    .protocol         = ProtocolVersion::Protocol2,
    .adcCount         = 2,
    .maxReceivers     = 7,
    .sampleRates      = {48000, 96000, 192000, 384000},
    .maxSampleRate    = 1536000,
    .attenuator       = {0, 31, 1, true},
    .preamp           = {true, true},
    .ocOutputCount    = 7,
    .hasAlexFilters   = true,
    .hasAlexTxRouting = true,
    .xvtrJackCount    = 1,
    .antennaInputCount = 3,
    .hasPureSignal    = true,
    .hasDiversityReceiver = true,
    .hasStepAttenuatorCal = true,
    .hasPaProfile     = true,
    .hasBandwidthMonitor = false,
    .hasIoBoardHl2    = false,
    .hasSidetoneGenerator = false,
    .minFirmwareVersion = 0,   // floor check removed; see file header
    .knownGoodFirmware  = 0,
    .displayName      = "ANAN-7000DLE/8000DLE (OrionMkII)",
    .sourceCitation   = "network.h:453, enums.cs:395, clsHardwareSpecific.cs:143-190",
};

// ─── HermesLite (HL2) ───────────────────────────────────────────────────────
// Source: network.h:454 (HermesLite=6), IoBoardHl2.cs, clsHardwareSpecific.cs:94-99
// ADC: SetRxADC(1) — single ADC. Protocol 1.
// HL2 supports 384k (Setup.cs:850-853 "The HL supports 384K", include_extra_p1_rate=true)
// Attenuator: HL2 uses LNA range -28..+32 (60 dB span), step 1
//   (Setup.cs:1084-1088 PerformDelayedInitialisation, :16085-16086 udHermesStepAttenuatorData)
//   Stored as minDb=0, maxDb=60 to represent the total span; the sign-aware
//   range [-28..+32] is handled at UI layer (not in capabilities struct).
// No Alex filters or OC outputs on HL2.
// hasBandwidthMonitor: HL2 has bandwidth_monitor via IoBoardHl2
//   (ChannelMaster/bandwidth_monitor.h + IoBoardHl2.cs)
// hasIoBoardHl2: true — IoBoardHl2.cs present and active for HL2
// hasSidetoneGenerator: true — HL2 firmware implements sidetone
//   (IoBoardHl2.cs references sidetone register)
// maxReceivers: 4 for HL2 (console.cs GetDDC HermesLite branch:8734, same as Hermes)
// Firmware: HL2 v72+ recommended (mi0bot Thetis-HL2 release notes)
constexpr BoardCapabilities kHermesLite = {
    .board            = HPSDRHW::HermesLite,
    .protocol         = ProtocolVersion::Protocol1,
    .adcCount         = 1,
    .maxReceivers     = 4,
    .sampleRates      = {48000, 96000, 192000, 384000},
    .maxSampleRate    = 384000,
    .attenuator       = {0, 60, 1, true},
    .preamp           = {false, false},
    .ocOutputCount    = 0,
    .hasAlexFilters   = false,
    .hasAlexTxRouting = false,
    .xvtrJackCount    = 0,
    .antennaInputCount = 1,
    .hasPureSignal    = false,
    .hasDiversityReceiver = false,
    .hasStepAttenuatorCal = false,
    .hasPaProfile     = false,
    .hasBandwidthMonitor = true,
    .hasIoBoardHl2    = true,
    .hasSidetoneGenerator = true,
    .minFirmwareVersion = 0,   // floor check removed; see file header
    .knownGoodFirmware  = 0,
    .displayName      = "Hermes Lite 2",
    .sourceCitation   = "network.h:454, IoBoardHl2.cs, Setup.cs:1082-1093,:16083-16086",
};

// ─── Saturn (ANAN-G2) ───────────────────────────────────────────────────────
// Source: network.h:455 (Saturn=10), enums.cs:397, clsHardwareSpecific.cs:164-176
// ADC: SetRxADC(2) — dual ADC. Protocol 2.
//   (clsHardwareSpecific.cs HasAudioAmplifier: Saturn = P2 protocol)
// P2 sample rates: up to 1536k (Setup.cs:854)
// Diversity + PureSignal: yes (console.cs GetDDC P2 Saturn branch:8598)
// PSDefaultPeak = 0.6121 for Saturn (clsHardwareSpecific.cs:323-324 P2 switch)
// Correction vs plan baseline: maxSampleRate updated to 1536000 (P2 board)
// TODO(3I-T2): verify firmware versions for Saturn/ANAN-G2
constexpr BoardCapabilities kSaturn = {
    .board            = HPSDRHW::Saturn,
    .protocol         = ProtocolVersion::Protocol2,
    .adcCount         = 2,
    .maxReceivers     = 7,
    .sampleRates      = {48000, 96000, 192000, 384000},
    .maxSampleRate    = 1536000,
    .attenuator       = {0, 31, 1, true},
    .preamp           = {true, true},
    .ocOutputCount    = 7,
    .hasAlexFilters   = true,
    .hasAlexTxRouting = true,
    .xvtrJackCount    = 1,
    .antennaInputCount = 3,
    .hasPureSignal    = true,
    .hasDiversityReceiver = true,
    .hasStepAttenuatorCal = true,
    .hasPaProfile     = true,
    .hasBandwidthMonitor = false,
    .hasIoBoardHl2    = false,
    .hasSidetoneGenerator = false,
    .minFirmwareVersion = 0,   // floor check removed; see file header
    .knownGoodFirmware  = 0,
    .displayName      = "ANAN-G2 (Saturn)",
    .sourceCitation   = "network.h:455, enums.cs:397, clsHardwareSpecific.cs:164-176",
};

// ─── SaturnMKII (ANAN-G2 MkII board revision) ───────────────────────────────
// Source: network.h:456 (SaturnMKII=11), enums.cs:398
// Comment in enums.cs: "ANAN-G2: MKII board?" — planned variant of Saturn.
// Capabilities identical to Saturn; protocol P2 by derivation from Saturn base.
// Correction vs plan baseline: maxSampleRate updated to 1536000 (P2 board)
// TODO(3I-T2): verify SaturnMKII vs Saturn capability differences when hardware ships
constexpr BoardCapabilities kSaturnMKII = {
    .board            = HPSDRHW::SaturnMKII,
    .protocol         = ProtocolVersion::Protocol2,
    .adcCount         = 2,
    .maxReceivers     = 7,
    .sampleRates      = {48000, 96000, 192000, 384000},
    .maxSampleRate    = 1536000,
    .attenuator       = {0, 31, 1, true},
    .preamp           = {true, true},
    .ocOutputCount    = 7,
    .hasAlexFilters   = true,
    .hasAlexTxRouting = true,
    .xvtrJackCount    = 1,
    .antennaInputCount = 3,
    .hasPureSignal    = true,
    .hasDiversityReceiver = true,
    .hasStepAttenuatorCal = true,
    .hasPaProfile     = true,
    .hasBandwidthMonitor = false,
    .hasIoBoardHl2    = false,
    .hasSidetoneGenerator = false,
    .minFirmwareVersion = 0,   // floor check removed; see file header
    .knownGoodFirmware  = 0,
    .displayName      = "ANAN-G2 MkII (SaturnMKII)",
    .sourceCitation   = "network.h:456, enums.cs:398 \"ANAN-G2: MKII board?\"",
};

// ─── Unknown (fallback) ─────────────────────────────────────────────────────
// Safe defaults for unrecognised boards. Used as forBoard() fallback.
constexpr BoardCapabilities kUnknown = {
    .board            = HPSDRHW::Unknown,
    .protocol         = ProtocolVersion::Protocol1,
    .adcCount         = 1,
    .maxReceivers     = 1,
    .sampleRates      = {48000, 0, 0, 0},
    .maxSampleRate    = 48000,
    .attenuator       = {0, 0, 0, false},
    .preamp           = {false, false},
    .ocOutputCount    = 0,
    .hasAlexFilters   = false,
    .hasAlexTxRouting = false,
    .xvtrJackCount    = 0,
    .antennaInputCount = 1,
    .hasPureSignal    = false,
    .hasDiversityReceiver = false,
    .hasStepAttenuatorCal = false,
    .hasPaProfile     = false,
    .hasBandwidthMonitor = false,
    .hasIoBoardHl2    = false,
    .hasSidetoneGenerator = false,
    .minFirmwareVersion = 0,
    .knownGoodFirmware  = 0,
    .displayName      = "Unknown board",
    .sourceCitation   = "fallback — no Thetis source for HPSDRHW::Unknown",
};

constexpr std::array kTable = {
    kAtlas, kHermes, kHermesII, kAngelia, kOrion,
    kOrionMKII, kHermesLite, kSaturn, kSaturnMKII, kUnknown
};

} // anonymous namespace

namespace BoardCapsTable {

const BoardCapabilities& forBoard(HPSDRHW hw) noexcept {
    for (const auto& e : kTable) {
        if (e.board == hw) { return e; }
    }
    return kUnknown;
}

const BoardCapabilities& forModel(HPSDRModel m) noexcept {
    return forBoard(boardForModel(m));
}

std::span<const BoardCapabilities> all() noexcept {
    return std::span<const BoardCapabilities>(kTable.data(), kTable.size());
}

// --- Per-model preamp item tables ---
// From Thetis console.cs:40755-40825 SetComboPreampForHPSDR().
// PreampMode mapping: 0=Off(-20dB HPSDR), 1=On(0dB), 2=Minus10, 3=Minus20,
//                     4=Minus30, 5=Minus40, 6=Minus50.

// on_off_preamp_settings = { "0dB", "-20dB" }
static constexpr PreampItem kOnOff[] = {
    {"0dB",   1},  // PreampMode::On
    {"-20dB", 0},  // PreampMode::Off
};

// anan100d_preamp_settings = { "0dB", "-10dB", "-20dB", "-30dB" }
static constexpr PreampItem kAnan100d[] = {
    {"0dB",   1},  // PreampMode::On
    {"-10dB", 2},  // PreampMode::Minus10
    {"-20dB", 3},  // PreampMode::Minus20
    {"-30dB", 4},  // PreampMode::Minus30
};

// alex_preamp_settings = { "-10db", "-20db", "-30db", "-40db", "-50db" }
// (lowercase "db" distinguishes ALEX modes from SA modes in Thetis)

// Combined: on_off + alex (for HPSDR/Hermes/ANAN100/etc with ALEX)
static constexpr PreampItem kOnOffPlusAlex[] = {
    {"0dB",   1},  // PreampMode::On
    {"-20dB", 0},  // PreampMode::Off
    {"-10db", 2},  // PreampMode::Minus10 (ALEX)
    {"-20db", 3},  // PreampMode::Minus20 (ALEX)
    {"-30db", 4},  // PreampMode::Minus30 (ALEX)
    {"-40db", 5},  // PreampMode::Minus40 (ALEX)
    {"-50db", 6},  // PreampMode::Minus50 (ALEX)
};

// Hermes Lite 2: preamp on only, no bypass/attenuation options.
static constexpr PreampItem kHl2[] = {
    {"0dB", 1},  // PreampMode::On
};

// Helper to produce dynamic-extent spans from static arrays.
template<std::size_t N>
static constexpr std::span<const PreampItem> items(const PreampItem (&a)[N]) noexcept {
    return {a, N};
}

std::span<const PreampItem> preampItemsForBoard(HPSDRHW hw, bool alexPresent) noexcept
{
    // From Thetis console.cs:40755 SetComboPreampForHPSDR — per-model switch.
    switch (hw) {
    case HPSDRHW::Atlas:
        return alexPresent ? items(kOnOffPlusAlex)
                           : items(kOnOff);

    case HPSDRHW::Hermes:
        // Hermes: with ALEX → on_off + alex; without → anan100d (4-step).
        // ANAN-10 uses Hermes board but always gets anan100d (no ALEX check).
        // ANAN-100 uses Hermes board but always gets on_off+alex.
        // Since we dispatch by HPSDRHW not HPSDRModel, and Hermes covers
        // both ANAN-10 and ANAN-100, we use the ALEX flag to differentiate.
        return alexPresent ? items(kOnOffPlusAlex)
                           : items(kAnan100d);

    case HPSDRHW::HermesII:
        // HermesII: ANAN-10E (no ALEX, anan100d) and ANAN-100B (always on_off+alex).
        return alexPresent ? items(kOnOffPlusAlex)
                           : items(kAnan100d);

    case HPSDRHW::Angelia:
        // ANAN-100D: with ALEX → on_off+alex; without → anan100d.
        return alexPresent ? items(kOnOffPlusAlex)
                           : items(kAnan100d);

    case HPSDRHW::Orion:
        // ANAN-200D: same as Angelia.
        return alexPresent ? items(kOnOffPlusAlex)
                           : items(kAnan100d);

    case HPSDRHW::OrionMKII:
    case HPSDRHW::Saturn:
        // ANAN-7000D/8000D/OrionMkII/G2/G2-1K/AnvelinaPro3: always anan100d.
        return items(kAnan100d);

    case HPSDRHW::HermesLite:
        // HL2: preamp on only, no variable attenuation.
        return items(kHl2);

    default:
        return items(kAnan100d);
    }
}

std::span<const PreampItem> rx2PreampItemsForBoard(HPSDRHW hw) noexcept
{
    // From Thetis console.cs:40815 — RX2 always uses either on_off or anan100d.
    switch (hw) {
    case HPSDRHW::Angelia:
    case HPSDRHW::Orion:
    case HPSDRHW::OrionMKII:
    case HPSDRHW::Saturn:
        return items(kAnan100d);
    default:
        return items(kOnOff);
    }
}

int stepAttMaxDb(HPSDRHW hw, bool alexPresent) noexcept
{
    // From Thetis setup.cs:15765 — 61 dB when ALEX present and board is NOT
    // in the exclusion list (ANAN-10/10E/7000D/8000D/OrionMkII/G2/G2-1K/
    // AnvelinaPro3/RedPitaya). NereusSDR maps by HPSDRHW, not HPSDRModel.
    // Exclusion list boards: OrionMKII, Saturn (G2/G2-1K), HermesLite.
    // Boards that CAN reach 61: Atlas, Hermes, HermesII, Angelia, Orion.
    if (!alexPresent) { return 31; }

    switch (hw) {
    case HPSDRHW::Atlas:
    case HPSDRHW::Hermes:
    case HPSDRHW::HermesII:
    case HPSDRHW::Angelia:
    case HPSDRHW::Orion:
        return 61;
    default:
        return 31;
    }
}

} // namespace BoardCapsTable
} // namespace NereusSDR
