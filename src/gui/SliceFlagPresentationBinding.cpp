// =================================================================
// src/gui/SliceFlagPresentationBinding.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-native lifetime boundary for a secondary slice's model-to-flag
// presentation wiring. There is no upstream AetherSDR equivalent for the
// multi-pan flag rehoming lifecycle exercised here (no port check applies).
//
// Modification history (NereusSDR):
//   2026-09-21 -- Extracted by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via OpenAI Codex.
//   2026-09-23 -- Frequency, mode, filter, AGC, gain, step and
//                 antenna handlers moved here with the flag as context
//                 (R-R3-30), by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
// =================================================================

#include "SliceFlagPresentationBinding.h"

#include "models/SliceModel.h"
#include "widgets/VfoWidget.h"

#include <QObject>

namespace NereusSDR {

QList<QMetaObject::Connection> wireSliceFlagPresentation(
    SliceModel* slice, VfoWidget* flag)
{
    if (!slice || !flag) {
        return {};
    }

    QList<QMetaObject::Connection> connections;
    connections.reserve(14);
    connections.append(QObject::connect(slice, &SliceModel::ritEnabledChanged,
                                         flag, &VfoWidget::setRitEnabled));
    connections.append(QObject::connect(slice, &SliceModel::ritHzChanged,
                                         flag, &VfoWidget::setRitHz));
    connections.append(QObject::connect(slice, &SliceModel::xitEnabledChanged,
                                         flag, &VfoWidget::setXitEnabled));
    connections.append(QObject::connect(slice, &SliceModel::xitHzChanged,
                                         flag, &VfoWidget::setXitHz));
    connections.append(QObject::connect(slice, &SliceModel::snbEnabledChanged,
                                         flag, &VfoWidget::setSnbEnabled));
    connections.append(QObject::connect(slice, &SliceModel::apfEnabledChanged,
                                         flag, &VfoWidget::setApfEnabled));
    connections.append(QObject::connect(slice, &SliceModel::apfTuneHzChanged,
                                         flag, &VfoWidget::setApfTuneHz));
    connections.append(QObject::connect(slice, &SliceModel::mutedChanged,
                                         flag, &VfoWidget::setMuted));
    connections.append(QObject::connect(slice, &SliceModel::audioPanChanged,
                                         flag, &VfoWidget::setAudioPan));
    connections.append(QObject::connect(slice, &SliceModel::ssqlEnabledChanged,
                                         flag, &VfoWidget::setSsqlEnabled));
    connections.append(QObject::connect(slice, &SliceModel::ssqlThreshChanged,
                                         flag, &VfoWidget::setSsqlThresh));
    connections.append(QObject::connect(slice, &SliceModel::agcThresholdChanged,
                                         flag, &VfoWidget::setAgcThreshold));
    connections.append(QObject::connect(slice, &SliceModel::binauralEnabledChanged,
                                         flag, &VfoWidget::setBinauralEnabled));
    connections.append(QObject::connect(slice, &SliceModel::lockedChanged,
                                         flag, &VfoWidget::setLocked));
    return connections;
}

QList<QMetaObject::Connection> wireSliceFlagStatePresentation(
    SliceModel* slice, VfoWidget* flag, SliceFlagHostHooks hooks)
{
    if (!slice || !flag) {
        return {};
    }

    // The flag is the context of every connection, so Qt disconnects them
    // before the flag is freed; the lambdas may therefore use it directly.
    QList<QMetaObject::Connection> connections;
    connections.reserve(9);
    connections.append(QObject::connect(slice, &SliceModel::frequencyChanged, flag,
        [flag, hook = std::move(hooks.frequencyChanged)](double hz) {
            flag->setFrequency(hz);
            if (hook) { hook(hz); }
        }));
    connections.append(QObject::connect(slice, &SliceModel::dspModeChanged, flag,
        [flag, hook = std::move(hooks.modeChanged)](DSPMode mode) {
            flag->setMode(mode);
            if (hook) { hook(mode); }
        }));
    connections.append(QObject::connect(slice, &SliceModel::filterChanged, flag,
        [flag, hook = std::move(hooks.filterChanged)](int low, int high) {
            flag->setFilter(low, high);
            if (hook) { hook(low, high); }
        }));
    connections.append(QObject::connect(slice, &SliceModel::agcModeChanged,
                                         flag, &VfoWidget::setAgcMode));
    connections.append(QObject::connect(slice, &SliceModel::afGainChanged,
                                         flag, &VfoWidget::setAfGain));
    connections.append(QObject::connect(slice, &SliceModel::rfGainChanged,
                                         flag, &VfoWidget::setRfGain));
    connections.append(QObject::connect(slice, &SliceModel::stepHzChanged,
                                         flag, &VfoWidget::setStepHz));
    connections.append(QObject::connect(slice, &SliceModel::rxAntennaChanged,
                                         flag, &VfoWidget::setRxAntenna));
    connections.append(QObject::connect(slice, &SliceModel::txAntennaChanged,
                                         flag, &VfoWidget::setTxAntenna));
    return connections;
}

}  // namespace NereusSDR
