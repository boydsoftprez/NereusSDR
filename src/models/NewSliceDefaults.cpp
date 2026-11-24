// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 J.J. Boyd (KG4VCF)
// no-port-check: NereusSDR-original.
// =================================================================
// src/models/NewSliceDefaults.cpp  (NereusSDR)
// =================================================================
//
// Reads and writes the Core's NewSlice/ settings and applies the ones an
// operator has changed onto a receiver they just asked for.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-24 - The operator's defaults for a slice they open.
//                NereusSDR-original. J.J. Boyd (KG4VCF), AI-assisted via
//                xAI Grok.
// =================================================================

#include "NewSliceDefaults.h"

#include "core/AppSettings.h"
#include "core/ControlRanges.h"
#include "models/BandDefaults.h"
#include "models/BandGrid.h"
#include "models/SliceModel.h"

#include <cmath>
#include <cstring>
#include <functional>

#include <QMetaProperty>

namespace NereusSDR {
namespace {

constexpr int kAgcTMin = -20;
constexpr int kAgcTMax = 120;

bool validMode(int mode)
{
    return mode >= static_cast<int>(DSPMode::LSB)
        && mode <= static_cast<int>(DSPMode::RADE2_L);
}

bool validBand(int band)
{
    return band == NewSliceDefaults::kFollowSlice
        || bandGridEntry(static_cast<Band>(band)) != nullptr;
}

const char* nrSlotKey(int nrSlot)
{
    switch (static_cast<NrSlot>(nrSlot)) {
    case NrSlot::NR1:  return "nr1";
    case NrSlot::NR2:  return "nr2";
    case NrSlot::NR3:  return "nr3";
    case NrSlot::NR4:  return "nr4";
    case NrSlot::DFNR: return "dfnr";
    case NrSlot::MNR:  return "mnr";
    case NrSlot::NNR:  return "nnr";
    default:           return nullptr;
    }
}

QString nrFieldKey(const char* slotKey, const char* field)
{
    return QStringLiteral("NewSlice/Nr/") + QLatin1String(slotKey)
        + QLatin1Char('/') + QLatin1String(field);
}

void writeSliceProperty(SliceModel& slice, const ControlRanges::NrControl& control, double value)
{
    const QMetaObject* meta = slice.metaObject();
    const int index = meta->indexOfProperty(control.property);
    if (index < 0) {
        return;
    }
    const QMetaProperty property = meta->property(index);
    QVariant outgoing;
    if (control.kind == ControlRanges::NrControlKind::Switch
        || property.userType() == QMetaType::Bool) {
        outgoing = value != 0.0;
    } else if (property.userType() == QMetaType::Double) {
        outgoing = value;
    } else {
        outgoing = static_cast<int>(std::lround(value));
    }
    if (!property.write(&slice, outgoing)) {
        property.write(&slice, static_cast<int>(std::lround(value)));
    }
}

void applyCustomNr(SliceModel& slice, int nrSlot)
{
    const ControlRanges::NrSlotControls* controls = NewSliceDefaults::nrControlsForChoice(nrSlot);
    // The Settings box only shows or hides these controls. A value the
    // operator has set is kept, and a new slice uses it, whether or not
    // the subsection is open.
    if (controls == nullptr) {
        return;
    }
    for (std::size_t i = 0; i < controls->count; ++i) {
        const ControlRanges::NrControl& control = controls->controls[i];
        double value = 0.0;
        if (!NewSliceDefaults::nrSetting(controls->key, control.property, &value)) {
            continue;
        }
        writeSliceProperty(slice, control, value);
    }
}

bool validAgcMode(int mode)
{
    return mode >= static_cast<int>(AGCMode::Off)
        && mode <= static_cast<int>(AGCMode::Custom);
}

bool validNr(int slot)
{
    return slot >= static_cast<int>(NrSlot::Off)
        && slot <= static_cast<int>(NrSlot::NNR);
}

bool validNb(int mode)
{
    return mode >= static_cast<int>(NbMode::Off)
        && mode <= static_cast<int>(NbMode::NB2);
}

bool validStep(int hz)
{
    return tuneStepIndexForHz(hz) >= 0;
}

bool validAf(int gain)
{
    return gain >= ControlRanges::kAfGainMin && gain <= ControlRanges::kAfGainMax;
}

bool validAgcT(int db)
{
    return db >= kAgcTMin && db <= kAgcTMax;
}

constexpr int kAutoAgcOffsetMin = -60;
constexpr int kAutoAgcOffsetMax = 60;

bool validAutoAgcOffset(int db)
{
    return db >= kAutoAgcOffsetMin && db <= kAutoAgcOffsetMax;
}

void writeInt(const QString& key, int value)
{
    AppSettings::instance().setValue(key, value);
}

bool validFilter(int policy)
{
    return policy == static_cast<int>(NewSliceDefaults::FilterPolicy::FollowSlice)
        || policy == static_cast<int>(NewSliceDefaults::FilterPolicy::ModeDefault);
}

bool validOnOffFollow(int policy)
{
    return policy == static_cast<int>(NewSliceDefaults::SwitchPolicy::Off)
        || policy == static_cast<int>(NewSliceDefaults::SwitchPolicy::On)
        || policy == NewSliceDefaults::kFollowSlice;
}

bool validOffOrFollow(int policy)
{
    return policy == static_cast<int>(NewSliceDefaults::SwitchPolicy::Off)
        || policy == NewSliceDefaults::kFollowSlice;
}

NewSliceDefaults::FilterPolicy readFilter()
{
    // The literal stays in the call so the settings-scope sweep sees it.
    const int stored = AppSettings::instance().value(
        QStringLiteral("NewSlice/Filter"),
        static_cast<int>(NewSliceDefaults::FilterPolicy::FollowSlice)).toInt();
    return validFilter(stored)
        ? static_cast<NewSliceDefaults::FilterPolicy>(stored)
        : NewSliceDefaults::FilterPolicy::FollowSlice;
}

NewSliceDefaults::SwitchPolicy readSwitch(const QString& key,
                                          NewSliceDefaults::SwitchPolicy fallback,
                                          bool (*ok)(int))
{
    const int stored = AppSettings::instance().value(
        key, static_cast<int>(fallback)).toInt();
    return ok(stored) ? static_cast<NewSliceDefaults::SwitchPolicy>(stored) : fallback;
}

void applySwitch(NewSliceDefaults::SwitchPolicy policy, bool seedOn,
                 const std::function<void(bool)>& set)
{
    if (policy == NewSliceDefaults::SwitchPolicy::FollowSlice) {
        set(seedOn);
        return;
    }
    set(policy == NewSliceDefaults::SwitchPolicy::On);
}

} // namespace

int NewSliceDefaults::modeChoice()
{
    const int stored = AppSettings::instance().value(
        QStringLiteral("NewSlice/Mode"), kFollowSlice).toInt();
    return (stored == kFollowSlice || validMode(stored)) ? stored : kFollowSlice;
}

void NewSliceDefaults::setModeChoice(int mode)
{
    if (mode != kFollowSlice && !validMode(mode)) {
        return;
    }
    writeInt(QStringLiteral("NewSlice/Mode"), mode);
}

int NewSliceDefaults::bandChoice()
{
    const int stored = AppSettings::instance().value(
        QStringLiteral("NewSlice/Band"), kFollowSlice).toInt();
    return validBand(stored) ? stored : kFollowSlice;
}

void NewSliceDefaults::setBandChoice(int band)
{
    if (!validBand(band)) {
        return;
    }
    writeInt(QStringLiteral("NewSlice/Band"), band);
}

const ControlRanges::NrSlotControls* NewSliceDefaults::nrControlsForChoice(int nrSlot)
{
    const char* key = nrSlotKey(nrSlot);
    if (key == nullptr) {
        return nullptr;
    }
    for (const ControlRanges::NrSlotControls& slot : ControlRanges::kNoiseReductionSlots) {
        if (std::strcmp(slot.key, key) == 0) {
            return &slot;
        }
    }
    return nullptr;
}

bool NewSliceDefaults::nrSettingsCustomized(const char* slotKey)
{
    if (slotKey == nullptr) {
        return false;
    }
    return AppSettings::instance().value(nrFieldKey(slotKey, "Custom"), 0).toInt() == 1;
}

void NewSliceDefaults::setNrSettingsCustomized(const char* slotKey, bool on)
{
    if (slotKey == nullptr) {
        return;
    }
    writeInt(nrFieldKey(slotKey, "Custom"), on ? 1 : 0);
}

bool NewSliceDefaults::nrSetting(const char* slotKey, const char* property, double* value)
{
    if (slotKey == nullptr || property == nullptr || value == nullptr) {
        return false;
    }
    const QString key = nrFieldKey(slotKey, property);
    if (!AppSettings::instance().contains(key)) {
        return false;
    }
    *value = AppSettings::instance().value(key).toDouble();
    return true;
}

void NewSliceDefaults::setNrSetting(const char* slotKey, const char* property, double value)
{
    if (slotKey == nullptr || property == nullptr) {
        return;
    }
    AppSettings::instance().setValue(nrFieldKey(slotKey, property), value);
}

NewSliceDefaults::FilterPolicy NewSliceDefaults::filterPolicy()
{
    return readFilter();
}

void NewSliceDefaults::setFilterPolicy(FilterPolicy policy)
{
    writeInt(QStringLiteral("NewSlice/Filter"), static_cast<int>(policy));
}

int NewSliceDefaults::agcModeChoice()
{
    const int stored = AppSettings::instance().value(
        QStringLiteral("NewSlice/AgcMode"), kFollowSlice).toInt();
    return (stored == kFollowSlice || validAgcMode(stored)) ? stored : kFollowSlice;
}

void NewSliceDefaults::setAgcModeChoice(int mode)
{
    if (mode != kFollowSlice && !validAgcMode(mode)) {
        return;
    }
    writeInt(QStringLiteral("NewSlice/AgcMode"), mode);
}

NewSliceDefaults::SwitchPolicy NewSliceDefaults::autoAgcChoice()
{
    return readSwitch(QStringLiteral("NewSlice/AutoAgc"),
                      SwitchPolicy::FollowSlice, validOnOffFollow);
}

void NewSliceDefaults::setAutoAgcChoice(SwitchPolicy policy)
{
    writeInt(QStringLiteral("NewSlice/AutoAgc"), static_cast<int>(policy));
}

int NewSliceDefaults::agcTChoice()
{
    const int stored = AppSettings::instance().value(
        QStringLiteral("NewSlice/AgcT"), kAgcTFollow).toInt();
    return (stored == kAgcTFollow || validAgcT(stored)) ? stored : kAgcTFollow;
}

void NewSliceDefaults::setAgcTChoice(int db)
{
    if (db != kAgcTFollow && !validAgcT(db)) {
        return;
    }
    writeInt(QStringLiteral("NewSlice/AgcT"), db);
}

int NewSliceDefaults::autoAgcOffsetChoice()
{
    const int stored = AppSettings::instance().value(
        QStringLiteral("NewSlice/AutoAgcOffset"), kAutoAgcOffsetFollow).toInt();
    return (stored == kAutoAgcOffsetFollow || validAutoAgcOffset(stored))
        ? stored : kAutoAgcOffsetFollow;
}

void NewSliceDefaults::setAutoAgcOffsetChoice(int db)
{
    if (db != kAutoAgcOffsetFollow && !validAutoAgcOffset(db)) {
        return;
    }
    writeInt(QStringLiteral("NewSlice/AutoAgcOffset"), db);
}

int NewSliceDefaults::nrChoice()
{
    const int stored = AppSettings::instance().value(
        QStringLiteral("NewSlice/Nr"), kFollowSlice).toInt();
    return (stored == kFollowSlice || validNr(stored)) ? stored : kFollowSlice;
}

void NewSliceDefaults::setNrChoice(int slot)
{
    if (slot != kFollowSlice && !validNr(slot)) {
        return;
    }
    writeInt(QStringLiteral("NewSlice/Nr"), slot);
}

NewSliceDefaults::SwitchPolicy NewSliceDefaults::anfChoice()
{
    return readSwitch(QStringLiteral("NewSlice/Anf"),
                      SwitchPolicy::FollowSlice, validOnOffFollow);
}

void NewSliceDefaults::setAnfChoice(SwitchPolicy policy)
{
    writeInt(QStringLiteral("NewSlice/Anf"), static_cast<int>(policy));
}

int NewSliceDefaults::nbChoice()
{
    const int stored = AppSettings::instance().value(
        QStringLiteral("NewSlice/Nb"), kFollowSlice).toInt();
    return (stored == kFollowSlice || validNb(stored)) ? stored : kFollowSlice;
}

void NewSliceDefaults::setNbChoice(int mode)
{
    if (mode != kFollowSlice && !validNb(mode)) {
        return;
    }
    writeInt(QStringLiteral("NewSlice/Nb"), mode);
}

NewSliceDefaults::SwitchPolicy NewSliceDefaults::snbChoice()
{
    return readSwitch(QStringLiteral("NewSlice/Snb"),
                      SwitchPolicy::FollowSlice, validOnOffFollow);
}

void NewSliceDefaults::setSnbChoice(SwitchPolicy policy)
{
    writeInt(QStringLiteral("NewSlice/Snb"), static_cast<int>(policy));
}

NewSliceDefaults::SwitchPolicy NewSliceDefaults::squelchChoice()
{
    return readSwitch(QStringLiteral("NewSlice/Squelch"),
                      SwitchPolicy::FollowSlice, validOffOrFollow);
}

void NewSliceDefaults::setSquelchChoice(SwitchPolicy policy)
{
    if (policy == SwitchPolicy::On) {
        return;
    }
    writeInt(QStringLiteral("NewSlice/Squelch"), static_cast<int>(policy));
}

int NewSliceDefaults::stepChoice()
{
    const int stored = AppSettings::instance().value(
        QStringLiteral("NewSlice/StepHz"), kFollowSlice).toInt();
    return (stored == kFollowSlice || validStep(stored)) ? stored : kFollowSlice;
}

void NewSliceDefaults::setStepChoice(int hz)
{
    if (hz != kFollowSlice && !validStep(hz)) {
        return;
    }
    writeInt(QStringLiteral("NewSlice/StepHz"), hz);
}

int NewSliceDefaults::afGainChoice()
{
    const int stored = AppSettings::instance().value(
        QStringLiteral("NewSlice/AfGain"), kFollowSlice).toInt();
    return (stored == kFollowSlice || validAf(stored)) ? stored : kFollowSlice;
}

void NewSliceDefaults::setAfGainChoice(int gain)
{
    if (gain != kFollowSlice && !validAf(gain)) {
        return;
    }
    writeInt(QStringLiteral("NewSlice/AfGain"), gain);
}

NewSliceDefaults::SwitchPolicy NewSliceDefaults::lockChoice()
{
    return readSwitch(QStringLiteral("NewSlice/Lock"),
                      SwitchPolicy::Off, validOnOffFollow);
}

void NewSliceDefaults::setLockChoice(SwitchPolicy policy)
{
    writeInt(QStringLiteral("NewSlice/Lock"), static_cast<int>(policy));
}

NewSliceDefaults::SwitchPolicy NewSliceDefaults::muteChoice()
{
    return readSwitch(QStringLiteral("NewSlice/Mute"),
                      SwitchPolicy::Off, validOnOffFollow);
}

void NewSliceDefaults::setMuteChoice(SwitchPolicy policy)
{
    writeInt(QStringLiteral("NewSlice/Mute"), static_cast<int>(policy));
}

NewSliceDefaults::SwitchPolicy NewSliceDefaults::ritChoice()
{
    return readSwitch(QStringLiteral("NewSlice/Rit"),
                      SwitchPolicy::Off, validOffOrFollow);
}

void NewSliceDefaults::setRitChoice(SwitchPolicy policy)
{
    if (policy == SwitchPolicy::On) {
        return;
    }
    writeInt(QStringLiteral("NewSlice/Rit"), static_cast<int>(policy));
}

NewSliceDefaults::SwitchPolicy NewSliceDefaults::xitChoice()
{
    return readSwitch(QStringLiteral("NewSlice/Xit"),
                      SwitchPolicy::Off, validOffOrFollow);
}

void NewSliceDefaults::setXitChoice(SwitchPolicy policy)
{
    if (policy == SwitchPolicy::On) {
        return;
    }
    writeInt(QStringLiteral("NewSlice/Xit"), static_cast<int>(policy));
}

void NewSliceDefaults::reset()
{
    auto& settings = AppSettings::instance();
    settings.remove(QStringLiteral("NewSlice/Mode"));
    settings.remove(QStringLiteral("NewSlice/Band"));
    const QStringList keys = settings.allKeys();
    for (const QString& key : keys) {
        if (key.startsWith(QLatin1String("NewSlice/Nr/"))) {
            settings.remove(key);
        }
    }
    settings.remove(QStringLiteral("NewSlice/Filter"));
    settings.remove(QStringLiteral("NewSlice/AgcMode"));
    settings.remove(QStringLiteral("NewSlice/AutoAgc"));
    settings.remove(QStringLiteral("NewSlice/AutoAgcOffset"));
    settings.remove(QStringLiteral("NewSlice/AgcT"));
    settings.remove(QStringLiteral("NewSlice/Nr"));
    settings.remove(QStringLiteral("NewSlice/Anf"));
    settings.remove(QStringLiteral("NewSlice/Nb"));
    settings.remove(QStringLiteral("NewSlice/Snb"));
    settings.remove(QStringLiteral("NewSlice/Squelch"));
    settings.remove(QStringLiteral("NewSlice/StepHz"));
    settings.remove(QStringLiteral("NewSlice/AfGain"));
    settings.remove(QStringLiteral("NewSlice/Lock"));
    settings.remove(QStringLiteral("NewSlice/Mute"));
    settings.remove(QStringLiteral("NewSlice/Rit"));
    settings.remove(QStringLiteral("NewSlice/Xit"));
}

void NewSliceDefaults::apply(SliceModel& slice, const SliceModel* seed)
{
    if (seed == &slice) {
        seed = nullptr;
    }
    const int band = bandChoice();
    if (band != kFollowSlice) {
        const BandSeed opening = BandDefaults::seedFor(static_cast<Band>(band));
        if (opening.valid) {
            slice.setFrequency(opening.frequencyHz);
            // The band's usual mode, unless Mode names one of its own.
            if (modeChoice() == kFollowSlice) {
                slice.setDspModeForNewReceiver(opening.mode);
            }
        }
    }
    const DSPMode modeBefore = slice.dspMode();
    const int mode = modeChoice();
    if (mode != kFollowSlice) {
        slice.setDspModeForNewReceiver(static_cast<DSPMode>(mode));
    }
    // A different mode cannot keep the other mode's filter edges. The
    // usual filter for the new mode is what a mode change already opens on.
    const bool modeMoved = slice.dspMode() != modeBefore;
    if (filterPolicy() == FilterPolicy::ModeDefault || modeMoved) {
        const auto edges = SliceModel::defaultFilterForMode(slice.dspMode());
        slice.setFilter(edges.first, edges.second);
    }

    const int agc = agcModeChoice();
    if (agc != kFollowSlice) {
        slice.setAgcMode(static_cast<AGCMode>(agc));
    }
    const SwitchPolicy autoAgc = autoAgcChoice();
    if (autoAgc == SwitchPolicy::On) {
        slice.setAutoAgcEnabled(true);
    } else if (autoAgc == SwitchPolicy::Off || agc != kFollowSlice) {
        // An AGC mode the operator named is that mode, not AUTO riding it.
        slice.setAutoAgcEnabled(false);
    }

    const int agcT = agcTChoice();
    if (agcT != kAgcTFollow) {
        slice.setRfGain(agcT);
    }
    const int autoOffset = autoAgcOffsetChoice();
    if (autoOffset != kAutoAgcOffsetFollow) {
        slice.setAutoAgcOffset(static_cast<double>(autoOffset));
    }

    const int nr = nrChoice();
    if (nr != kFollowSlice) {
        slice.setActiveNr(static_cast<NrSlot>(nr));
        applyCustomNr(slice, nr);
    }
    const bool seedAnf = seed && seed->anfEnabled();
    if (anfChoice() != SwitchPolicy::FollowSlice) {
        applySwitch(anfChoice(), seedAnf, [&](bool on) { slice.setAnfEnabled(on); });
    }

    const int nb = nbChoice();
    if (nb != kFollowSlice) {
        slice.setNbMode(static_cast<NbMode>(nb));
    }
    const bool seedSnb = seed && seed->snbEnabled();
    if (snbChoice() != SwitchPolicy::FollowSlice) {
        applySwitch(snbChoice(), seedSnb, [&](bool on) { slice.setSnbEnabled(on); });
    }
    if (squelchChoice() == SwitchPolicy::Off) {
        slice.setSsqlEnabled(false);
        slice.setAmsqEnabled(false);
        slice.setFmsqEnabled(false);
    }

    const int step = stepChoice();
    if (step != kFollowSlice) {
        slice.setStepHz(step);
    }
    const int af = afGainChoice();
    if (af != kFollowSlice) {
        slice.setAfGain(af);
    }

    const bool seedLock = seed && seed->locked();
    applySwitch(lockChoice(), seedLock, [&](bool on) { slice.setLocked(on); });
    const bool seedMute = seed && seed->muted();
    applySwitch(muteChoice(), seedMute, [&](bool on) { slice.setMuted(on); });

    if (ritChoice() == SwitchPolicy::FollowSlice) {
        if (seed) {
            slice.setRitHz(seed->ritHz());
            slice.setRitEnabled(seed->ritEnabled());
        }
    } else {
        slice.setRitEnabled(false);
        slice.setRitHz(0);
    }
    if (xitChoice() == SwitchPolicy::FollowSlice) {
        if (seed) {
            slice.setXitHz(seed->xitHz());
            slice.setXitEnabled(seed->xitEnabled());
        }
    } else {
        slice.setXitEnabled(false);
        slice.setXitHz(0);
    }
}

} // namespace NereusSDR
