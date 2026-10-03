#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// src/core/DisturbanceCheck.h  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 75 (R-IOS-30; the several-devices design,
// docs/architecture/2026-09-24-several-devices-on-one-core-design.md,
// sections 7.1 and 7.2, rulings 7.1 to 7.3 and 7.8).
//
// Some settings belong to the radio, not to one slice: the sample rate, an
// ADC's attenuator and preamp, the receive antenna, a shared receiver's
// noise blanker, the notches, PureSignal, the amplifier and tuner. A change
// to one of them reaches every slice that listens through what it touches,
// whoever owns it, so before the Core applies such a change it asks the
// person making it, naming each other device it would disturb (D53).
//
// This class answers the one question that needs: given what a change
// touches (its Scope) and how the Core is laid out now (its Topology), which
// other devices does it disturb, and what happens to each of their slices?
//
//   changes                  the slice keeps receiving, differently;
//   moves                    it goes to another receiver;
//   closes                   it closes;
//   pausesWhileTransmitting  it stops while the transmit holder transmits
//                            (PureSignal on a board with one ADC).
//
// The requester's own slices never count, and neither do slices nobody
// owns (there is nobody to ask). When the change touches the transmitter,
// the transmit holder is disturbed too, listed with holdsTransmit true and,
// when that is all that touches it, no slices (ruling 7.8).
//
// A sample-rate change is not worked out here: the Core simulates it with
// today's plan (RadioModel::planSampleRateReach) and hands the result in as
// Scope::planned, one effect per slice the plan moves, closes or changes.
//
// Pure: it reads only what it is given and changes nothing.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), iPhone app plan Task 75 (R-IOS-30), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-26: Transmit group fix wave 2: every holder on the air counts,
//               the station device's own keys included (onAirHolder),
//               exempt by change not by holder; ruling 8.11's freeze on
//               every path (XIT, pan moves, a stored change at proceed); a
//               hosting desktop's key named after it. J.J. Boyd (KG4VCF),
//               with AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QSet>
#include <QString>

namespace NereusSDR {

class DisturbanceCheck {
public:
    enum class Effect { Changes, Moves, Closes, PausesWhileTransmitting };

    /// One live slice as the Core sees it.
    struct SliceInfo {
        int sliceId = -1;
        /// Whose it is (SliceOwnership's subject: the owner, or the device
        /// a held slice is kept for); empty for a slice nobody owns.
        QByteArray device;
        /// Its receiver, from 0; -1 when it has none.
        int stream = -1;
        /// The ADC that receiver is fed from (RadioModel::adcForStream).
        int adc = -1;
        double frequencyHz = 0.0;
        /// Its passband, relative to frequencyHz.
        int filterLowHz = 0;
        int filterHighHz = 0;
    };

    /// Who holds transmit (Task 34's TransmitHolder). Empty until that task
    /// joins: nobody holds it, so the transmitter disturbs nobody.
    struct Transmit {
        QByteArray holder;
        bool keyed = false;
        int txSliceId = -1;
        /// Ruling 8.11: the transmit slice while the station device is
        /// keyed, whoever owns it; -1 otherwise. No take, move or rate
        /// change may touch it.
        int frozenSliceId = -1;
        /// Whether the holder can be asked and told (ruling 7.8). False for
        /// the station device, which has no session: it still counts for
        /// refusedOnAir, but a change it alone would disturb applies.
        bool holderAskable = true;
    };

    struct Topology {
        QList<SliceInfo> slices;
        Transmit transmit;
    };

    /// A band of frequencies, both edges included (a notch).
    struct Range {
        double lowHz = 0.0;
        double highHz = 0.0;
    };

    /// What a change touches. The parts add up: a slice any part reaches
    /// is disturbed.
    struct Scope {
        /// Every slice (the radio choice; a Protocol 1 sample rate).
        bool radio = false;
        /// Every slice on a receiver fed by one of these ADCs.
        QSet<int> adcs;
        /// Every slice on one of these receivers.
        QSet<int> receivers;
        /// Every slice whose passband overlaps one of these.
        QList<Range> ranges;
        /// The transmitter: its holder is disturbed.
        bool transmitter = false;
        /// Ruling 7.4: a change to the transmit path (the amplifier, the
        /// tuner, an antenna, PureSignal, the interlock, the power cap)
        /// waits while the holder is on the air.
        bool transmitPath = false;
        /// Ruling 7.4: a Protocol 1 sample rate stops the radio's data
        /// flow, so it too waits while the holder is on the air.
        bool stopsDataFlow = false;
        /// What happens to a slice the parts above reach.
        Effect effect = Effect::Changes;
        /// A sample-rate change's plan: each slice it reaches, with its
        /// effect. Wins over `effect` for the slices it names, and reaches
        /// them whatever the parts above say.
        QHash<int, Effect> planned;
    };

    struct AffectedSlice {
        int sliceId = -1;
        Effect effect = Effect::Changes;
    };

    /// One disturbed device, with its slices in topology order.
    struct Affected {
        QByteArray device;
        bool holdsTransmit = false;
        QList<AffectedSlice> slices;
    };

    /// Each device other than `requester` the change disturbs, in the order
    /// its first slice appears in `topology` (the holder last when only the
    /// transmitter reaches it).
    static QList<Affected> check(const Scope& scope, const Topology& topology,
                                 const QByteArray& requester);

    /// Ruling 7.4 (D60): whether the change must be refused because the
    /// transmit holder, another device than `requester`, is on the air: it
    /// touches the transmit path, stops the radio's data flow, or would
    /// move or close the holder's transmit slice, whoever owns that slice
    /// (the requester's own included). Ruling 8.11: a rate change that
    /// reaches the frozen slice at all is refused too. `affected` is
    /// check()'s answer for the same change. Never true while nobody holds
    /// transmit.
    static bool refusedOnAir(const Scope& scope, const Topology& topology,
                             const QByteArray& requester, const QList<Affected>& affected);

    /// Whether `slice` is reached by the parts of `scope` (not `planned`).
    static bool reaches(const Scope& scope, const SliceInfo& slice);

    /// "changes", "moves", "closes", "pausesWhileTransmitting".
    static QString effectName(Effect effect);
};

} // namespace NereusSDR
