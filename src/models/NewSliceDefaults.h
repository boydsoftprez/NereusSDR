#pragma once
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 J.J. Boyd (KG4VCF)
// no-port-check: NereusSDR-original.
// =================================================================
// src/models/NewSliceDefaults.h  (NereusSDR)
// =================================================================
//
// The settings a slice someone asked for opens with (+RX, Ctrl+R, Add
// slice on the pan). They live on the Core (NewSlice/ in AppSettings).
// A row left at its usual choice keeps today's behaviour: the new slice
// is a copy of the slice the operator is on, and lock, mute, RIT and XIT
// start off.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-24 - The operator's defaults for a slice they open.
//                NereusSDR-original. J.J. Boyd (KG4VCF), AI-assisted via
//                xAI Grok.
// =================================================================

#include "core/WdspTypes.h"

namespace NereusSDR {

class SliceModel;

// Choices for opening a new receiver. Stored under NewSlice/ on the Core.
class NewSliceDefaults {
public:
    // A row follows the slice the operator is on. Lock, mute, RIT and XIT
    // do not: their usual choice is Off.
    static constexpr int kFollowSlice = -1;

    // AGC-T's "follow the slice" value. -1 sits inside the real -20..120
    // range, so it cannot mean "follow".
    static constexpr int kAgcTFollow = -999;

    // AUTO AGC ± Offset's "follow the slice" value. The real range is
    // -60..60, so -1 cannot mean "follow".
    static constexpr int kAutoAgcOffsetFollow = -999;

    // The volume and AGC-T rows' "a value I choose" item. Never stored.
    static constexpr int kSpecify = -2;

    enum class FilterPolicy : int {
        FollowSlice = 0,
        ModeDefault = 1,
    };

    // Off and On are the stored switch. FollowSlice is kFollowSlice.
    enum class SwitchPolicy : int {
        Off = 0,
        On = 1,
        FollowSlice = kFollowSlice,
    };

    static int modeChoice();
    static void setModeChoice(int mode);

    static FilterPolicy filterPolicy();
    static void setFilterPolicy(FilterPolicy policy);

    static int agcModeChoice();
    static void setAgcModeChoice(int mode);

    static SwitchPolicy autoAgcChoice();
    static void setAutoAgcChoice(SwitchPolicy policy);

    static int agcTChoice();
    static void setAgcTChoice(int db);

    // AUTO AGC ± Offset, the dB shift from the noise floor. kAutoAgcOffsetFollow
    // follows the slice you are on.
    static int autoAgcOffsetChoice();
    static void setAutoAgcOffsetChoice(int db);

    static int nrChoice();
    static void setNrChoice(int slot);

    static SwitchPolicy anfChoice();
    static void setAnfChoice(SwitchPolicy policy);

    static int nbChoice();
    static void setNbChoice(int mode);

    static SwitchPolicy snbChoice();
    static void setSnbChoice(SwitchPolicy policy);

    static SwitchPolicy squelchChoice();
    static void setSquelchChoice(SwitchPolicy policy);

    static int stepChoice();
    static void setStepChoice(int hz);

    static int afGainChoice();
    static void setAfGainChoice(int gain);

    static SwitchPolicy lockChoice();
    static void setLockChoice(SwitchPolicy policy);

    static SwitchPolicy muteChoice();
    static void setMuteChoice(SwitchPolicy policy);

    static SwitchPolicy ritChoice();
    static void setRitChoice(SwitchPolicy policy);

    static SwitchPolicy xitChoice();
    static void setXitChoice(SwitchPolicy policy);

    // Puts every row back on its usual choice.
    static void reset();

    // Applies the rows that are not on their usual choice. `seed` is the
    // slice the new one was copied from, or null when there was none.
    // Frequency and sample rate are left as the caller set them.
    static void apply(SliceModel& slice, const SliceModel* seed);
};

} // namespace NereusSDR
