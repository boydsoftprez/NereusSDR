// =================================================================
// src/gui/widgets/AntennaPickerMenu.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original; no upstream port. See header for description.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-27 Created in C++20/Qt6 for NereusSDR by J.J. Boyd (KG4VCF),
//              with AI-assisted transformation via Anthropic Claude Code.
// =================================================================
//
// no-port-check: NereusSDR-original

#include "gui/widgets/AntennaPickerMenu.h"

#include "core/BoardCapabilities.h"
#include "core/accessories/AlexController.h"
#include "models/Band.h"
#include "models/SliceModel.h"

#include <QAction>

namespace NereusSDR {

AntennaPickerMenu::AntennaPickerMenu(SliceModel* slice,
                                     AlexController* /*alex*/,
                                     const BoardCapabilities& caps,
                                     QWidget* parent)
    : QMenu(parent)
{
    // SliceModel doesn't currently expose a public band accessor; derive
    // the band from its frequency via the canonical Band:: free function.
    const Band sliceBand = bandFromFrequency(slice->frequency());
    addSection(QStringLiteral("Slice %1 - Antenna for %2")
                   .arg(slice->sliceLetter())
                   .arg(bandLabel(sliceBand)));

    QStringList antennas = {QStringLiteral("ANT1"), QStringLiteral("ANT2"), QStringLiteral("ANT3")};
    if (caps.antennaInputCount < 3) {
        antennas.removeLast();
    }
    if (caps.antennaInputCount < 2) {
        antennas.removeLast();
    }

    const QString currentAnt = slice->rxAntenna();
    for (const QString& ant : antennas) {
        QString consequence;
        if (ant == currentAnt) {
            consequence = QStringLiteral("Chain %1 - current").arg(slice->chainIndex());
        } else {
            // Full consequence text (would joining other chain bypass? would
            // re-route be needed?) lands when the chain-conflict policy is
            // wired in Sub-Epic E Tasks 8-13 (design section 5). For now we
            // show a simple "(switches chain)" hint.
            consequence = QStringLiteral("(switches chain)");
        }
        QAction* act = addAction(QStringLiteral("%1   %2").arg(ant, consequence));
        if (ant == currentAnt) {
            act->setCheckable(true);
            act->setChecked(true);
        }
        connect(act, &QAction::triggered, this, [this, slice, ant]() {
            emit antennaSelected(slice->sliceIndex(), ant);
        });
    }

    addSeparator();
    addAction(QStringLiteral("EXT1   RX only"));
    addAction(QStringLiteral("EXT2   RX only"));
    addAction(QStringLiteral("BYPS   RX only, no Alex"));
}

AntennaPickerMenu::~AntennaPickerMenu() = default;

} // namespace NereusSDR
