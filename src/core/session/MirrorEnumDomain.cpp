// =================================================================
// src/core/session/MirrorEnumDomain.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. Remote-daemon R2, whole-branch
// review finding (Important 3).
//
// See MirrorEnumDomain.h for why this table is hand-declared rather than
// reflected, and for what default deny costs.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-09  J.J. Boyd / KG4VCF  Whole-branch review, Important 3:
//                                    declared enum domains. AI-assisted
//                                    transformation via Anthropic Claude
//                                    Code.
//   2026-09-23  J.J. Boyd / KG4VCF  R-R3-45: SliceModel::OutputRoute
//                                    (speakers or headphones). AI-assisted
//                                    implementation via Anthropic Claude
//                                    Code.
//   2026-09-23  J.J. Boyd / KG4VCF  R-R3-47: AmplifierModel::State.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  R-R3-45: SliceModel::OutputRoute.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-24 - R-R3-47 / R-R3-48: TunerModel::BandFollow and
//                RfKitModel::TunerMode. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-24 - R-R3-47 / R-R3-22: AccessoryDataModel::InterlockMode.
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  Merge: lane B already declared
//                                    SliceModel::OutputRoute; the
//                                    duplicate declaration is removed.
//                                    AI-assisted via Anthropic Claude
//                                    Code.
//   2026-09-24 - R-R3-49 (parity Task 2): DrivePowerSource (the tune
//                drive source on `transmit`). J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-28 - Band::Band2m (R-IOS-26, R-R3-49). J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/session/MirrorEnumDomain.h"

#include "core/WdspTypes.h"
#include "models/AccessoryDataModel.h"
#include "models/AmplifierModel.h"
#include "models/RfKitModel.h"
#include "models/Band.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"
#include "models/TunerModel.h"

#include <QHash>

#include <initializer_list>

namespace NereusSDR {

namespace {

using DomainTable = QHash<int, QList<qlonglong>>;

// Keyed on QMetaType::id() rather than on QMetaType itself: the ids for
// these custom types are assigned at registration and stable for the life
// of the process, which is the only scope this table has, and QHash needs
// a key type with qHash.
template <typename E>
void declare(DomainTable* table, std::initializer_list<E> values)
{
    QList<qlonglong> declared;
    declared.reserve(static_cast<int>(values.size()));
    for (const E value : values) {
        declared.append(static_cast<qlonglong>(value));
    }
    table->insert(QMetaType::fromType<E>().id(), declared);
}

// Every enum type reachable through a Q_PROPERTY on a mirrored class.
// Twelve properties across ten types, all of them on SliceModel today
// (MirrorSchema.h's own count); TransmitModel, TunerModel, RadioModel and
// PanadapterModel declare no enum property at all.
//
// Written out enumerator by enumerator rather than as a first/last range,
// so every name here is compile-checked against the real enum. See the
// header for the one drift this still cannot catch.
const DomainTable& table()
{
    // Function-local static: built once, on first use, thread-safe
    // initialisation. QMetaType::fromType<E>().id() is a runtime call, so
    // this cannot be a constexpr table.
    static const DomainTable kTable = [] {
        DomainTable t;

        // From src/core/WdspTypes.h. RADE_U / RADE_L are the two
        // NereusSDR-native modes; they are declared values of this enum
        // and a remote peer may select them, even though WDSP has no
        // knowledge of them (RxChannel::wdspModeFor maps them before the
        // WDSP call).
        declare<DSPMode>(&t, { DSPMode::LSB, DSPMode::USB, DSPMode::DSB, DSPMode::CWL,
                               DSPMode::CWU, DSPMode::FM, DSPMode::AM, DSPMode::DIGU,
                               DSPMode::SPEC, DSPMode::DIGL, DSPMode::SAM, DSPMode::DRM,
                               DSPMode::RADE_U, DSPMode::RADE_L });

        declare<AGCMode>(&t, { AGCMode::Off, AGCMode::Long, AGCMode::Slow, AGCMode::Med,
                               AGCMode::Fast, AGCMode::Custom });

        declare<NbMode>(&t, { NbMode::Off, NbMode::NB, NbMode::NB2 });

        declare<NrSlot>(&t, { NrSlot::Off, NrSlot::NR1, NrSlot::NR2, NrSlot::NR3,
                              NrSlot::NR4, NrSlot::DFNR, NrSlot::BNR, NrSlot::MNR,
                              NrSlot::NNR });

        declare<NrPosition>(&t, { NrPosition::PreAgc, NrPosition::PostAgc });

        declare<EmnrGainMethod>(&t, { EmnrGainMethod::Linear, EmnrGainMethod::Log,
                                      EmnrGainMethod::Gamma, EmnrGainMethod::Trained });

        declare<EmnrNpeMethod>(&t, { EmnrNpeMethod::Osms, EmnrNpeMethod::Mmse,
                                     EmnrNpeMethod::Nstat });

        declare<SbnrAlgo>(&t, { SbnrAlgo::Algo1, SbnrAlgo::Algo2, SbnrAlgo::Algo3 });

        declare<FmTxMode>(&t, { FmTxMode::High, FmTxMode::Simplex, FmTxMode::Low });

        // R-R3-45: the output a receiver plays on (VAX design 6.2).
        declare<SliceModel::OutputRoute>(&t, { SliceModel::OutputRoute::Speakers,
                                               SliceModel::OutputRoute::Headphones });

        declare<TunerModel::ConnectionPhase>(&t, {
            TunerModel::ConnectionPhase::Disabled,
            TunerModel::ConnectionPhase::Disconnected,
            TunerModel::ConnectionPhase::Discovering,
            TunerModel::ConnectionPhase::Connecting,
            TunerModel::ConnectionPhase::Identifying,
            TunerModel::ConnectionPhase::Retrying,
            TunerModel::ConnectionPhase::Connected,
            TunerModel::ConnectionPhase::Error,
        });

        // R-R3-47: the Power Genius's state on the `amplifier` object.
        // The `amplifier` and `rfkit` connection phases are
        // TunerModel::ConnectionPhase, declared above.
        declare<AmplifierModel::State>(&t, {
            AmplifierModel::State::Unknown,
            AmplifierModel::State::PowerUp,
            AmplifierModel::State::Standby,
            AmplifierModel::State::Idle,
            AmplifierModel::State::Operate,
            AmplifierModel::State::TransmitA,
            AmplifierModel::State::TransmitB,
            AmplifierModel::State::Fault,
        });

        // R-R3-48: band follow on the `amplifier` and `rfkit` objects.
        declare<TunerModel::BandFollow>(&t, {
            TunerModel::BandFollow::Off,
            TunerModel::BandFollow::Waiting,
            TunerModel::BandFollow::Following,
            TunerModel::BandFollow::ThisComputerOnly,
        });

        // R-R3-47: the RF-Kit tuner's mode on the `rfkit` object.
        declare<RfKitModel::TunerMode>(&t, {
            RfKitModel::TunerMode::Unknown,
            RfKitModel::TunerMode::Bypass,
            RfKitModel::TunerMode::Manual,
            RfKitModel::TunerMode::AutoTuning,
            RfKitModel::TunerMode::Auto,
        });

        // R-R3-47 / R-R3-22: the transmit interlock mode on `accessoryData`.
        declare<AccessoryDataModel::InterlockMode>(&t, {
            AccessoryDataModel::InterlockMode::Disabled,
            AccessoryDataModel::InterlockMode::Warn,
            AccessoryDataModel::InterlockMode::Block,
        });

        // R-R3-49 (parity Task 2): the tune drive source on `transmit`.
        declare<DrivePowerSource>(&t, { DrivePowerSource::DriveSlider,
                                        DrivePowerSource::TuneSlider,
                                        DrivePowerSource::Fixed });

        // From src/models/Band.h. Count is deliberately ABSENT: it is an
        // iteration bound and AlexController's "no slice in this slot"
        // sentinel, not a band, and accepting it here would let a remote
        // peer put a sentinel into a band field. SwlFirst and SwlLast are
        // aliases of Band120m and Band11m, so they need no entry of their
        // own.
        declare<Band>(&t, { Band::Band160m, Band::Band80m, Band::Band60m, Band::Band40m,
                            Band::Band30m, Band::Band20m, Band::Band17m, Band::Band15m,
                            Band::Band12m, Band::Band10m, Band::Band6m, Band::GEN,
                            Band::WWV, Band::XVTR, Band::Band120m, Band::Band90m,
                            Band::Band61m, Band::Band49m, Band::Band41m, Band::Band31m,
                            Band::Band25m, Band::Band22m, Band::Band19m, Band::Band16m,
                            Band::Band14m, Band::Band13m, Band::Band11m,
                            // 2 m (R-IOS-26, R-R3-49): the Core sends it only
                            // to a peer that declares band2m (BandLinkFit.h).
                            Band::Band2m });

        return t;
    }();
    return kTable;
}

} // namespace

bool MirrorEnumDomain::contains(QMetaType metaType, qlonglong value)
{
    const auto it = table().constFind(metaType.id());
    if (it == table().constEnd()) {
        return false;
    }
    return it.value().contains(value);
}

bool MirrorEnumDomain::hasDomain(QMetaType metaType)
{
    return table().contains(metaType.id());
}

QList<qlonglong> MirrorEnumDomain::valuesFor(QMetaType metaType)
{
    return table().value(metaType.id());
}

} // namespace NereusSDR
