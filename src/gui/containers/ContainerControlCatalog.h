#pragma once
// no-port-check: NereusSDR-original creation metadata for existing native actions.
// Modification history (NereusSDR):
//   2026-10-03 — Individually arranged controls by J.J. Boyd (KG4VCF),
//                 AI-assisted via OpenAI Codex.
#include "gui/meters/OtherButtonItem.h"
#include <QSize>

namespace NereusSDR {
struct ContainerControlDescriptor {
    QString creationId;
    QString title;
    OtherButtonItem::ButtonId buttonId;
};
// Creation shortcuts only. Documents store ordinary OTHERBTNS records; the IDs,
// rendering and command routing remain OtherButtonItem's existing native ones.
inline QVector<ContainerControlDescriptor> supportedContainerControls()
{
    using Id = OtherButtonItem::ButtonId;
    return {{"control.mox", "MOX", Id::Mox}, {"control.tune", "Tune", Id::Tun},
            {"control.monitor", "Monitor", Id::Mon}, {"control.twoTone", "2Tone", Id::TwoTon},
            {"control.pureSignal", "PureSignal", Id::PsA}, {"control.anf", "ANF", Id::Anf},
            {"control.snb", "SNB", Id::Snb}, {"control.mnf", "MNF", Id::Mnf},
            {"control.peak", "Peak", Id::PeakHold}, {"control.ctun", "CTUN", Id::Ctun},
            {"control.vax1", "VAX 1", Id::Vac1}, {"control.vax2", "VAX 2", Id::Vac2},
            {"control.mute", "Mute", Id::Mute}, {"control.binaural", "Binaural", Id::Bin},
            {"control.duplex", "Duplex", Id::Dup}};
}
inline bool isSingleContainerControl(const MeterItem* item)
{
    const auto* other = qobject_cast<const OtherButtonItem*>(item);
    if (!other || other->columns() != 1) { return false; }
    for (const auto& control : supportedContainerControls()) {
        if (other->visibleBits() == (1u << int(control.buttonId))) { return true; }
    }
    return false;
}
inline QSize singleContainerControlSize() { return {112, 44}; }
inline QSize singleContainerControlMinimum() { return {64, 32}; }
} // namespace NereusSDR
