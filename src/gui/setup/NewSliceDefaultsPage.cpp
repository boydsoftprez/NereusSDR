// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 J.J. Boyd (KG4VCF)
// no-port-check: NereusSDR-original.
// =================================================================
// src/gui/setup/NewSliceDefaultsPage.cpp  (NereusSDR)
// =================================================================
//
// Setup > General > New Slice. Writes the Core's NewSlice/ settings.
// The Core applies them when it mints a slice someone asked for.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-24 - The operator's defaults for a slice they open.
//                NereusSDR-original. J.J. Boyd (KG4VCF), AI-assisted via
//                xAI Grok.
// =================================================================

#include "NewSliceDefaultsPage.h"

#include "core/ControlRanges.h"
#include "core/WdspTypes.h"
#include "models/BandGrid.h"
#include "models/NewSliceDefaults.h"
#include "models/SliceModel.h"

#include <algorithm>
#include <cmath>

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSpinBox>

namespace NereusSDR {
namespace {

constexpr int kAgcTMin = -20;
constexpr int kAgcTMax = 120;

void keepComboNarrow(QComboBox* box)
{
    box->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    box->setMinimumContentsLength(16);
}

} // namespace

NewSliceDefaultsPage::NewSliceDefaultsPage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("New Slice"), model, parent)
{
    auto* intro = new QLabel(
        tr("A new slice opens with these settings. Each new slice stays on the "
           "frequency you are tuned to, unless a band is chosen below, and it "
           "keeps the sample rate of the receiver it joins. A setting left on "
           "its default choice follows the slice you are on. Lock, mute, RIT "
           "and XIT still start off until you change them. These settings "
           "belong to the Core, so every window on this station opens a new "
           "slice the same way."),
        this);
    intro->setWordWrap(true);
    intro->setObjectName(QStringLiteral("newSliceIntro"));
    addContent(intro);

    addSection(tr("Receiver"));
    const QString follow = tr("Same as the slice I am on");
    m_band = addChoice(tr("Band"), QStringLiteral("newSliceBand"));
    addItem(m_band, follow, NewSliceDefaults::kFollowSlice);
    for (const BandGridEntry& entry : kBandGrid) {
        addItem(m_band, QString::fromLatin1(entry.name), static_cast<int>(entry.band));
    }
    m_band->setToolTip(tr("Choosing a band opens the new slice on that band's usual "
                          "frequency. Its usual mode is used unless Mode is set below."));

    m_mode = addChoice(tr("Mode"), QStringLiteral("newSliceMode"));
    addItem(m_mode, follow, NewSliceDefaults::kFollowSlice);
    for (int mode = static_cast<int>(DSPMode::LSB);
         mode <= static_cast<int>(DSPMode::RADE2_L); ++mode) {
        addItem(m_mode, SliceModel::modeName(static_cast<DSPMode>(mode)), mode);
    }
    m_mode->setToolTip(tr("Choosing a mode opens the new slice on that mode's usual filter."));

    m_filter = addChoice(tr("Filter"), QStringLiteral("newSliceFilter"));
    addItem(m_filter, follow, static_cast<int>(NewSliceDefaults::FilterPolicy::FollowSlice));
    addItem(m_filter, tr("The usual filter for the mode"),
            static_cast<int>(NewSliceDefaults::FilterPolicy::ModeDefault));
    m_filter->setToolTip(
        tr("Same as the slice you are on applies when the mode stays that slice's mode. "
           "A different mode opens on its usual filter."));

    m_agcMode = addChoice(tr("AGC"), QStringLiteral("newSliceAgcMode"));
    addItem(m_agcMode, follow, NewSliceDefaults::kFollowSlice);
    addItem(m_agcMode, tr("Off"), static_cast<int>(AGCMode::Off));
    addItem(m_agcMode, tr("Long"), static_cast<int>(AGCMode::Long));
    addItem(m_agcMode, tr("Slow"), static_cast<int>(AGCMode::Slow));
    addItem(m_agcMode, tr("Med"), static_cast<int>(AGCMode::Med));
    addItem(m_agcMode, tr("Fast"), static_cast<int>(AGCMode::Fast));
    addItem(m_agcMode, tr("Custom"), static_cast<int>(AGCMode::Custom));
    m_agcMode->setToolTip(tr("Choosing a mode turns AUTO off. Turn AUTO on below to keep it."));

    m_autoAgc = addChoice(tr("AUTO AGC"), QStringLiteral("newSliceAutoAgc"));
    addItem(m_autoAgc, follow, NewSliceDefaults::kFollowSlice);
    addItem(m_autoAgc, tr("Off"), static_cast<int>(NewSliceDefaults::SwitchPolicy::Off));
    addItem(m_autoAgc, tr("On"), static_cast<int>(NewSliceDefaults::SwitchPolicy::On));

    // The same ± Offset as DSP > AGC/ALC: the dB shift from the noise floor.
    m_agcOffsetChoice = addChoice(tr("± Offset"), QStringLiteral("newSliceAgcOffsetChoice"));
    addItem(m_agcOffsetChoice, follow, NewSliceDefaults::kAutoAgcOffsetFollow);
    addItem(m_agcOffsetChoice, tr("This value"), NewSliceDefaults::kSpecify);
    m_agcOffsetChoice->setToolTip(tr("dB shift from noise floor"));
    m_agcOffset = addLabeledSpinner(tr("± Offset value"), -60, 60, 20);
    m_agcOffset->setObjectName(QStringLiteral("newSliceAgcOffset"));
    m_agcOffset->setSuffix(tr(" dB"));
    m_agcOffset->setToolTip(tr("dB shift from noise floor"));

    m_agcTChoice = addChoice(tr("AGC-T"), QStringLiteral("newSliceAgcTChoice"));
    addItem(m_agcTChoice, follow, NewSliceDefaults::kAgcTFollow);
    addItem(m_agcTChoice, tr("This value"), NewSliceDefaults::kSpecify);
    m_agcT = addLabeledSpinner(tr("AGC-T value"), kAgcTMin, kAgcTMax, 80);
    m_agcT->setObjectName(QStringLiteral("newSliceAgcT"));
    m_agcT->setSuffix(tr(" dB"));

    m_step = addChoice(tr("Tune step"), QStringLiteral("newSliceStep"));
    addItem(m_step, follow, NewSliceDefaults::kFollowSlice);
    for (int i = 0; i < kTuneStepListSize; ++i) {
        addItem(m_step, QString::fromLatin1(kTuneStepList[i].name), kTuneStepList[i].stepHz);
    }

    m_volumeChoice = addChoice(tr("Volume"), QStringLiteral("newSliceVolumeChoice"));
    addItem(m_volumeChoice, follow, NewSliceDefaults::kFollowSlice);
    addItem(m_volumeChoice, tr("This level"), NewSliceDefaults::kSpecify);
    m_volume = addLabeledSpinner(tr("Volume level"),
                                 ControlRanges::kAfGainMin, ControlRanges::kAfGainMax, 50);
    m_volume->setObjectName(QStringLiteral("newSliceVolume"));

    addSection(tr("Noise"));
    m_nr = addChoice(tr("Noise reduction"), QStringLiteral("newSliceNr"));
    addItem(m_nr, follow, NewSliceDefaults::kFollowSlice);
    const struct { const char* name; NrSlot slot; } reducers[] = {
        {"Off", NrSlot::Off},   {"NR", NrSlot::NR1},   {"NR2", NrSlot::NR2},
        {"NR3", NrSlot::NR3},   {"NR4", NrSlot::NR4},  {"DFNR", NrSlot::DFNR},
        {"BNR", NrSlot::BNR},   {"MNR", NrSlot::MNR},  {"NNR", NrSlot::NNR},
    };
    for (const auto& reducer : reducers) {
        addItem(m_nr, QString::fromLatin1(reducer.name), static_cast<int>(reducer.slot));
    }
    m_nr->setToolTip(tr("Settings shows this reducer's controls. A value you set is kept."));
    installNrDetails();

    m_anf = addChoice(tr("ANF"), QStringLiteral("newSliceAnf"));
    addItem(m_anf, follow, NewSliceDefaults::kFollowSlice);
    addItem(m_anf, tr("Off"), static_cast<int>(NewSliceDefaults::SwitchPolicy::Off));
    addItem(m_anf, tr("On"), static_cast<int>(NewSliceDefaults::SwitchPolicy::On));

    m_nb = addChoice(tr("Noise blanker"), QStringLiteral("newSliceNb"));
    addItem(m_nb, follow, NewSliceDefaults::kFollowSlice);
    addItem(m_nb, tr("Off"), static_cast<int>(NbMode::Off));
    addItem(m_nb, tr("NB"), static_cast<int>(NbMode::NB));
    addItem(m_nb, tr("NB2"), static_cast<int>(NbMode::NB2));

    m_snb = addChoice(tr("SNB"), QStringLiteral("newSliceSnb"));
    addItem(m_snb, follow, NewSliceDefaults::kFollowSlice);
    addItem(m_snb, tr("Off"), static_cast<int>(NewSliceDefaults::SwitchPolicy::Off));
    addItem(m_snb, tr("On"), static_cast<int>(NewSliceDefaults::SwitchPolicy::On));

    m_squelch = addChoice(tr("Squelch"), QStringLiteral("newSliceSquelch"));
    addItem(m_squelch, follow, NewSliceDefaults::kFollowSlice);
    addItem(m_squelch, tr("Off"), static_cast<int>(NewSliceDefaults::SwitchPolicy::Off));
    m_squelch->setToolTip(tr("Off closes SSB, AM and FM squelch. Their thresholds stay."));

    addSection(tr("Lock, mute, RIT and XIT"));
    m_lock = addChoice(tr("Lock"), QStringLiteral("newSliceLock"));
    m_mute = addChoice(tr("Mute"), QStringLiteral("newSliceMute"));
    for (QComboBox* box : {m_lock, m_mute}) {
        addItem(box, tr("Off"), static_cast<int>(NewSliceDefaults::SwitchPolicy::Off));
        addItem(box, follow, NewSliceDefaults::kFollowSlice);
        addItem(box, tr("On"), static_cast<int>(NewSliceDefaults::SwitchPolicy::On));
    }
    m_rit = addChoice(tr("RIT"), QStringLiteral("newSliceRit"));
    m_xit = addChoice(tr("XIT"), QStringLiteral("newSliceXit"));
    for (QComboBox* box : {m_rit, m_xit}) {
        addItem(box, tr("Off"), static_cast<int>(NewSliceDefaults::SwitchPolicy::Off));
        addItem(box, follow, NewSliceDefaults::kFollowSlice);
        box->setToolTip(tr("Same as the slice you are on also takes its offset."));
    }

    auto* reset = new QPushButton(tr("Reset to default settings"), this);
    reset->setObjectName(QStringLiteral("newSliceReset"));
    connect(reset, &QPushButton::clicked, this, [this] {
        NewSliceDefaults::reset();
        reload();
    });
    addContent(reset);

    const auto onChoice = [this](int) {
        if (!m_reloading) {
            save();
        }
    };
    for (QComboBox* box : {m_band, m_mode, m_filter, m_agcMode, m_autoAgc, m_agcOffsetChoice,
                           m_agcTChoice, m_step, m_volumeChoice, m_anf, m_nb,
                           m_snb, m_squelch, m_lock, m_mute, m_rit, m_xit}) {
        connect(box, qOverload<int>(&QComboBox::currentIndexChanged), this, onChoice);
    }
    connect(m_nr, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
        if (m_reloading) {
            return;
        }
        // The checkmark stays as the operator left it. Only the open
        // subsection follows the reducer they picked, and save keeps that
        // choice without touching the knobs they already set.
        showNrDetails();
        save();
    });
    connect(m_nrCustomize, &QCheckBox::toggled, this, [this](bool) {
        showNrDetails();
    });
    connect(m_agcOffset, qOverload<int>(&QSpinBox::valueChanged), this, onChoice);
    connect(m_agcT, qOverload<int>(&QSpinBox::valueChanged), this, onChoice);
    connect(m_volume, qOverload<int>(&QSpinBox::valueChanged), this, onChoice);

    reload();
}

QComboBox* NewSliceDefaultsPage::addChoice(const QString& label, const QString& objectName)
{
    auto* box = addLabeledCombo(label, {});
    box->setObjectName(objectName);
    keepComboNarrow(box);
    return box;
}

void NewSliceDefaultsPage::addItem(QComboBox* box, const QString& label, int value)
{
    box->addItem(label, value);
}

void NewSliceDefaultsPage::select(QComboBox* box, int value)
{
    const int index = box->findData(value);
    box->setCurrentIndex(index >= 0 ? index : 0);
}

void NewSliceDefaultsPage::syncFromModel()
{
    SetupPage::syncFromModel();
    reload();
}

void NewSliceDefaultsPage::showEvent(QShowEvent* event)
{
    SetupPage::showEvent(event);
    reload();
}

void NewSliceDefaultsPage::reload()
{
    if (!m_mode) {
        return;
    }
    const bool was = m_reloading;
    m_reloading = true;
    select(m_band, NewSliceDefaults::bandChoice());
    select(m_mode, NewSliceDefaults::modeChoice());
    select(m_filter, static_cast<int>(NewSliceDefaults::filterPolicy()));
    select(m_agcMode, NewSliceDefaults::agcModeChoice());
    select(m_autoAgc, static_cast<int>(NewSliceDefaults::autoAgcChoice()));
    const int autoOffset = NewSliceDefaults::autoAgcOffsetChoice();
    select(m_agcOffsetChoice, autoOffset == NewSliceDefaults::kAutoAgcOffsetFollow
                                  ? NewSliceDefaults::kAutoAgcOffsetFollow
                                  : NewSliceDefaults::kSpecify);
    if (autoOffset != NewSliceDefaults::kAutoAgcOffsetFollow) {
        m_agcOffset->setValue(autoOffset);
    }
    m_agcOffset->setEnabled(autoOffset != NewSliceDefaults::kAutoAgcOffsetFollow);
    const int agcT = NewSliceDefaults::agcTChoice();
    select(m_agcTChoice, agcT == NewSliceDefaults::kAgcTFollow
                             ? NewSliceDefaults::kAgcTFollow
                             : NewSliceDefaults::kSpecify);
    if (agcT != NewSliceDefaults::kAgcTFollow) {
        m_agcT->setValue(agcT);
    }
    m_agcT->setEnabled(agcT != NewSliceDefaults::kAgcTFollow);
    select(m_step, NewSliceDefaults::stepChoice());
    const int af = NewSliceDefaults::afGainChoice();
    select(m_volumeChoice, af == NewSliceDefaults::kFollowSlice
                               ? NewSliceDefaults::kFollowSlice
                               : NewSliceDefaults::kSpecify);
    if (af != NewSliceDefaults::kFollowSlice) {
        m_volume->setValue(af);
    }
    m_volume->setEnabled(af != NewSliceDefaults::kFollowSlice);
    select(m_nr, NewSliceDefaults::nrChoice());
    syncNrDetails();
    select(m_anf, static_cast<int>(NewSliceDefaults::anfChoice()));
    select(m_nb, NewSliceDefaults::nbChoice());
    select(m_snb, static_cast<int>(NewSliceDefaults::snbChoice()));
    select(m_squelch, static_cast<int>(NewSliceDefaults::squelchChoice()));
    select(m_lock, static_cast<int>(NewSliceDefaults::lockChoice()));
    select(m_mute, static_cast<int>(NewSliceDefaults::muteChoice()));
    select(m_rit, static_cast<int>(NewSliceDefaults::ritChoice()));
    select(m_xit, static_cast<int>(NewSliceDefaults::xitChoice()));
    m_reloading = was;
}

void NewSliceDefaultsPage::save()
{
    const auto chosen = [](const QComboBox* box) { return box->currentData().toInt(); };
    NewSliceDefaults::setBandChoice(chosen(m_band));
    NewSliceDefaults::setModeChoice(chosen(m_mode));
    NewSliceDefaults::setFilterPolicy(
        static_cast<NewSliceDefaults::FilterPolicy>(chosen(m_filter)));
    NewSliceDefaults::setAgcModeChoice(chosen(m_agcMode));
    NewSliceDefaults::setAutoAgcChoice(
        static_cast<NewSliceDefaults::SwitchPolicy>(chosen(m_autoAgc)));
    if (chosen(m_agcOffsetChoice) == NewSliceDefaults::kSpecify) {
        m_agcOffset->setEnabled(true);
        NewSliceDefaults::setAutoAgcOffsetChoice(m_agcOffset->value());
    } else {
        m_agcOffset->setEnabled(false);
        NewSliceDefaults::setAutoAgcOffsetChoice(NewSliceDefaults::kAutoAgcOffsetFollow);
    }
    if (chosen(m_agcTChoice) == NewSliceDefaults::kSpecify) {
        m_agcT->setEnabled(true);
        NewSliceDefaults::setAgcTChoice(m_agcT->value());
    } else {
        m_agcT->setEnabled(false);
        NewSliceDefaults::setAgcTChoice(NewSliceDefaults::kAgcTFollow);
    }
    NewSliceDefaults::setNrChoice(chosen(m_nr));
    NewSliceDefaults::setAnfChoice(
        static_cast<NewSliceDefaults::SwitchPolicy>(chosen(m_anf)));
    NewSliceDefaults::setNbChoice(chosen(m_nb));
    NewSliceDefaults::setSnbChoice(
        static_cast<NewSliceDefaults::SwitchPolicy>(chosen(m_snb)));
    NewSliceDefaults::setSquelchChoice(
        static_cast<NewSliceDefaults::SwitchPolicy>(chosen(m_squelch)));
    NewSliceDefaults::setStepChoice(chosen(m_step));
    if (chosen(m_volumeChoice) == NewSliceDefaults::kSpecify) {
        m_volume->setEnabled(true);
        NewSliceDefaults::setAfGainChoice(m_volume->value());
    } else {
        m_volume->setEnabled(false);
        NewSliceDefaults::setAfGainChoice(NewSliceDefaults::kFollowSlice);
    }
    NewSliceDefaults::setLockChoice(
        static_cast<NewSliceDefaults::SwitchPolicy>(chosen(m_lock)));
    NewSliceDefaults::setMuteChoice(
        static_cast<NewSliceDefaults::SwitchPolicy>(chosen(m_mute)));
    NewSliceDefaults::setRitChoice(
        static_cast<NewSliceDefaults::SwitchPolicy>(chosen(m_rit)));
    NewSliceDefaults::setXitChoice(
        static_cast<NewSliceDefaults::SwitchPolicy>(chosen(m_xit)));
}

void NewSliceDefaultsPage::installNrDetails()
{
    // The Noise reduction label is 150 wide, then 8 of row spacing. A
    // further 12 puts Settings just inside the combo, with room above
    // and below so it does not sit against the rows around it.
    auto* block = new QWidget(this);
    block->setObjectName(QStringLiteral("newSliceNrSettings"));
    auto* blockLayout = new QVBoxLayout(block);
    blockLayout->setContentsMargins(170, 8, 0, 8);
    blockLayout->setSpacing(8);

    m_nrCustomize = new QCheckBox(tr("Settings"), block);
    m_nrCustomize->setObjectName(QStringLiteral("newSliceNrCustomize"));
    m_nrCustomize->setVisible(false);
    blockLayout->addWidget(m_nrCustomize);

    m_nrDetails = new QWidget(block);
    m_nrDetails->setObjectName(QStringLiteral("newSliceNrDetails"));
    auto* detailsLayout = new QVBoxLayout(m_nrDetails);
    detailsLayout->setContentsMargins(0, 0, 0, 0);
    detailsLayout->setSpacing(4);

    const auto remember = [this](QWidget* editor, const QString& slotKey, const QString& property) {
        const auto saveEdit = [this, editor, slotKey, property] {
            if (m_reloading || editor == nullptr) {
                return;
            }
            double value = 0.0;
            if (auto* spin = qobject_cast<QDoubleSpinBox*>(editor)) {
                value = spin->value() * spin->property("nrToProperty").toDouble();
            } else if (auto* spin = qobject_cast<QSpinBox*>(editor)) {
                value = spin->value() * spin->property("nrToProperty").toDouble();
            } else if (auto* combo = qobject_cast<QComboBox*>(editor)) {
                value = combo->currentData().toDouble();
            } else if (auto* box = qobject_cast<QCheckBox*>(editor)) {
                value = box->isChecked() ? 1.0 : 0.0;
            } else {
                return;
            }
            NewSliceDefaults::setNrSetting(slotKey.toUtf8().constData(),
                                           property.toUtf8().constData(), value);
        };
        if (auto* spin = qobject_cast<QDoubleSpinBox*>(editor)) {
            connect(spin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, saveEdit);
        } else if (auto* spin = qobject_cast<QSpinBox*>(editor)) {
            connect(spin, qOverload<int>(&QSpinBox::valueChanged), this, saveEdit);
        } else if (auto* combo = qobject_cast<QComboBox*>(editor)) {
            connect(combo, qOverload<int>(&QComboBox::currentIndexChanged), this, saveEdit);
        } else if (auto* box = qobject_cast<QCheckBox*>(editor)) {
            connect(box, &QCheckBox::toggled, this, saveEdit);
        }
    };

    for (const ControlRanges::NrSlotControls& slot : ControlRanges::kNoiseReductionSlots) {
        auto* panel = new QWidget(m_nrDetails);
        panel->setObjectName(QStringLiteral("newSliceNrPanel_%1").arg(QLatin1String(slot.key)));
        auto* panelLayout = new QVBoxLayout(panel);
        panelLayout->setContentsMargins(0, 0, 0, 0);
        panelLayout->setSpacing(4);
        for (std::size_t i = 0; i < slot.count; ++i) {
            const ControlRanges::NrControl& control = slot.controls[i];
            const QString name = QStringLiteral("newSliceNr_%1_%2")
                .arg(QLatin1String(slot.key), QLatin1String(control.property));
            const bool fractional = control.decimals > 0 || control.step < 1.0 || control.divide != 1;
            const double toProperty = fractional
                ? static_cast<double>(control.divide) * control.scale
                : control.scale;
            QWidget* editor = nullptr;
            if (control.kind == ControlRanges::NrControlKind::Switch) {
                auto* box = new QCheckBox(QString::fromLatin1(control.label), panel);
                box->setObjectName(name);
                box->setChecked(control.defaultValue != 0.0);
                panelLayout->addWidget(box);
                editor = box;
            } else if (control.kind == ControlRanges::NrControlKind::Choice) {
                auto* row = new QHBoxLayout;
                auto* label = new QLabel(QString::fromLatin1(control.label), panel);
                label->setStyleSheet(QStringLiteral("QLabel { color: #c8d8e8; font-size: 12px; }"));
                label->setFixedWidth(160);
                label->setWordWrap(true);
                auto* combo = new QComboBox(panel);
                combo->setObjectName(name);
                keepComboNarrow(combo);
                for (std::size_t n = 0; n < control.optionCount; ++n) {
                    combo->addItem(QString::fromLatin1(control.options[n].label), control.options[n].id);
                }
                const int index = combo->findData(static_cast<int>(control.defaultValue));
                combo->setCurrentIndex(index >= 0 ? index : 0);
                row->addWidget(label);
                row->addWidget(combo, 1);
                panelLayout->addLayout(row);
                editor = combo;
            } else if (fractional) {
                auto* row = new QHBoxLayout;
                auto* label = new QLabel(QString::fromLatin1(control.label), panel);
                label->setStyleSheet(QStringLiteral("QLabel { color: #c8d8e8; font-size: 12px; }"));
                label->setFixedWidth(160);
                label->setWordWrap(true);
                auto* spin = new QDoubleSpinBox(panel);
                spin->setObjectName(name);
                spin->setRange(control.min / control.divide, control.max / control.divide);
                spin->setSingleStep(control.step / control.divide);
                spin->setDecimals(std::max(control.decimals, 0));
                spin->setSuffix(QString::fromLatin1(control.suffix));
                spin->setProperty("nrToProperty", toProperty);
                spin->setValue(control.defaultValue / toProperty);
                row->addWidget(label);
                row->addWidget(spin, 1);
                panelLayout->addLayout(row);
                editor = spin;
            } else {
                auto* row = new QHBoxLayout;
                auto* label = new QLabel(QString::fromLatin1(control.label), panel);
                label->setStyleSheet(QStringLiteral("QLabel { color: #c8d8e8; font-size: 12px; }"));
                label->setFixedWidth(160);
                label->setWordWrap(true);
                auto* spin = new QSpinBox(panel);
                spin->setObjectName(name);
                spin->setRange(static_cast<int>(control.min), static_cast<int>(control.max));
                spin->setSingleStep(std::max(1, static_cast<int>(control.step)));
                spin->setSuffix(QString::fromLatin1(control.suffix));
                spin->setProperty("nrToProperty", toProperty);
                spin->setValue(static_cast<int>(std::lround(control.defaultValue / toProperty)));
                row->addWidget(label);
                row->addWidget(spin, 1);
                panelLayout->addLayout(row);
                editor = spin;
            }
            remember(editor, QString::fromLatin1(slot.key), QString::fromLatin1(control.property));
        }
        panel->setVisible(false);
        detailsLayout->addWidget(panel);
    }
    m_nrDetails->setVisible(false);
    blockLayout->addWidget(m_nrDetails);
    addToSection(block);
}

void NewSliceDefaultsPage::showNrDetails()
{
    if (!m_nr || !m_nrCustomize || !m_nrDetails) {
        return;
    }
    const ControlRanges::NrSlotControls* controls =
        NewSliceDefaults::nrControlsForChoice(m_nr->currentData().toInt());
    const bool offer = controls != nullptr;
    if (QWidget* block = m_nrCustomize->parentWidget()) {
        block->setVisible(offer);
    }
    m_nrCustomize->setVisible(offer);
    m_nrDetails->setVisible(offer && m_nrCustomize->isChecked());
    const auto panels = m_nrDetails->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
    for (QWidget* panel : panels) {
        const bool selected = controls != nullptr
            && panel->objectName() == QStringLiteral("newSliceNrPanel_%1").arg(QLatin1String(controls->key));
        panel->setVisible(selected);
    }
}

void NewSliceDefaultsPage::syncNrDetails()
{
    if (!m_nr || !m_nrCustomize || !m_nrDetails) {
        return;
    }
    for (const ControlRanges::NrSlotControls& slot : ControlRanges::kNoiseReductionSlots) {
        for (std::size_t i = 0; i < slot.count; ++i) {
            const ControlRanges::NrControl& control = slot.controls[i];
            double value = control.defaultValue;
            NewSliceDefaults::nrSetting(slot.key, control.property, &value);
            const QString name = QStringLiteral("newSliceNr_%1_%2")
                .arg(QLatin1String(slot.key), QLatin1String(control.property));
            if (auto* box = m_nrDetails->findChild<QCheckBox*>(name)) {
                const QSignalBlocker blocker(box);
                box->setChecked(value != 0.0);
            } else if (auto* combo = m_nrDetails->findChild<QComboBox*>(name)) {
                const QSignalBlocker blocker(combo);
                const int index = combo->findData(static_cast<int>(std::lround(value)));
                combo->setCurrentIndex(index >= 0 ? index : 0);
            } else if (auto* spin = m_nrDetails->findChild<QDoubleSpinBox*>(name)) {
                const QSignalBlocker blocker(spin);
                const double toProperty = spin->property("nrToProperty").toDouble();
                if (toProperty != 0.0) {
                    spin->setValue(value / toProperty);
                }
            } else if (auto* spin = m_nrDetails->findChild<QSpinBox*>(name)) {
                const QSignalBlocker blocker(spin);
                const double toProperty = spin->property("nrToProperty").toDouble();
                if (toProperty != 0.0) {
                    spin->setValue(static_cast<int>(std::lround(value / toProperty)));
                }
            }
        }
    }
    // The checkmark is only show and hide. A reload must not clear it,
    // and must not put back a value the operator has already set.
    showNrDetails();
}

} // namespace NereusSDR
