// src/core/BoardCapabilities.h
//
// Pure-data capability registry. One entry per HPSDRHW value.
// Drives both protocol connections and the Hardware setup UI.
//
// Source: mi0bot/Thetis-HL2 clsHardwareSpecific.cs, Setup.cs, console.cs,
//         HPSDR/specHPSDR.cs, ChannelMaster/network.h

#pragma once

#include "HpsdrModel.h"
#include "RadioDiscovery.h"  // for ProtocolVersion
#include <array>
#include <span>

namespace NereusSDR {

struct BoardCapabilities {
    HPSDRHW         board;
    ProtocolVersion protocol;

    int  adcCount;
    int  maxReceivers;
    std::array<int, 4> sampleRates;  // zero-pad unused slots
    int  maxSampleRate;

    struct Atten {
        int minDb;
        int maxDb;
        int stepDb;
        bool present;
    } attenuator;

    struct Preamp {
        bool present;
        bool hasBypassAndPreamp;
    } preamp;

    int  ocOutputCount;
    bool hasAlexFilters;
    bool hasAlexTxRouting;
    int  xvtrJackCount;
    int  antennaInputCount;

    bool hasPureSignal;
    bool hasDiversityReceiver;
    bool hasStepAttenuatorCal;
    bool hasPaProfile;

    bool hasBandwidthMonitor;
    bool hasIoBoardHl2;
    bool hasSidetoneGenerator;

    int  minFirmwareVersion;
    int  knownGoodFirmware;

    const char* displayName;
    const char* sourceCitation;
};

namespace BoardCapsTable {
    const BoardCapabilities& forBoard(HPSDRHW hw) noexcept;
    const BoardCapabilities& forModel(HPSDRModel m) noexcept;
    std::span<const BoardCapabilities> all() noexcept;

    // --- Per-model preamp/attenuator helpers ---
    // Porting from Thetis console.cs:40755-40825 SetComboPreampForHPSDR().

    // Preamp combo item: display text + underlying PreampMode-like index.
    struct PreampItem {
        const char* label;   // e.g. "0dB", "-10dB", "-20db" (case from Thetis)
        int         modeInt; // index into NereusSDR PreampMode (0=Off..6=Minus50)
    };

    // Returns the RX1 preamp combo items for a given board + ALEX presence.
    // From Thetis console.cs:40755 SetComboPreampForHPSDR.
    std::span<const PreampItem> preampItemsForBoard(HPSDRHW hw, bool alexPresent) noexcept;

    // Returns the RX2 preamp combo items for a given board.
    // From Thetis console.cs:40815 comboRX2Preamp population.
    std::span<const PreampItem> rx2PreampItemsForBoard(HPSDRHW hw) noexcept;

    // Returns the step attenuator maximum dB for a given board + ALEX presence.
    // From Thetis setup.cs:15765 udHermesStepAttenuatorData max logic.
    // 31 for most boards, 61 for ALEX-equipped boards not in the exclusion list.
    int stepAttMaxDb(HPSDRHW hw, bool alexPresent) noexcept;
}

} // namespace NereusSDR
