#pragma once
// no-port-check: NereusSDR-original creation metadata for existing native actions.
// Modification history (NereusSDR):
//   2026-10-26 - containerStackGridHeight(): a grid alone in a short view
//                 shows whole rows, never a half row. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-25 - A single control is one shown button, by Thetis button
//                 number. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-10-04 — Shared stack button-grid sizing by J.J. Boyd (KG4VCF),
//                 AI-assisted via OpenAI Codex.
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
    if (!other || other->columns() != 1 || other->shownButtonCount() != 1) { return false; }
    // Exactly one button drawn, and it is one of the controls. Buttons saved
    // visible but never drawn (no action, feature not built) do not count:
    // an earlier record always held two such (Spectrascope and Off).
    for (const auto& control : supportedContainerControls()) {
        if (other->isButtonShown(control.buttonId)) { return true; }
    }
    return false;
}
inline QSize singleContainerControlSize() { return {112, 44}; }
inline QSize singleContainerControlMinimum() { return {64, 32}; }
// Stack presentation only; saved masks, columns and legacy aspect ratios survive.
inline int containerStackButtonRows(const ButtonBoxItem* box)
{
    if (!box || box->columns()<=0 || box->columns()>box->buttonCount()) { return 0; }
    const int shown=box->shownButtonCount();
    return qMax(1,shown/box->columns()+int(shown%box->columns()!=0));
}
inline QSize containerStackButtonSize(const MeterItem* item)
{
    const auto* box=qobject_cast<const ButtonBoxItem*>(item);
    const int rows=containerStackButtonRows(box);
    if (!rows) { return {}; }
    const QSize cell=singleContainerControlSize();
    return {box->columns()*cell.width(),rows*cell.height()};
}
// The height a stack grid of `rows` rows takes when `available` px of the
// container's view hold it alone: its preferred rows when they fit; rows
// shrunk to fill the view while they stay at least the minimum row; else
// rows sized so the view holds a whole number of them, never a half row
// at its bottom edge (the rest scroll into view a row at a time).
inline int containerStackGridHeight(int rows, int available)
{
    const int preferred = singleContainerControlSize().height();
    const int minimum = singleContainerControlMinimum().height();
    if (rows <= 0 || available <= 0 || available >= rows * preferred) { return rows * preferred; }
    if (available >= rows * minimum) { return available; }
    const int whole = qMax(1, available / minimum);
    return qRound(double(available) * rows / whole);
}
inline QSize containerStackButtonMinimum(const MeterItem* item)
{
    const auto* box=qobject_cast<const ButtonBoxItem*>(item);
    const int rows=containerStackButtonRows(box);
    if (!rows) { return {}; }
    const QSize cell=singleContainerControlMinimum();
    return {box->columns()*cell.width(),rows*cell.height()};
}
} // namespace NereusSDR
