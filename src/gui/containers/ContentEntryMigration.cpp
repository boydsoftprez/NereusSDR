// no-port-check: NereusSDR-original migration of saved container entries.
// Modification history (NereusSDR):
//   2026-10-26 - An entry still named its type's old picker label takes
//                 the new label on load; a name the operator chose is
//                 kept. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-10-25 - Written: a Longpath needle or bar's "direction" becomes
//                 "binding" on load. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
#include "ContentEntryMigration.h"

#include "gui/instruments/LongpathContent.h"

namespace NereusSDR {

namespace {

void migrateLongpathDirection(ContentEntry& entry)
{
    if (entry.typeId != QLatin1String(LongpathContent::kNeedleType)
        && entry.typeId != QLatin1String(LongpathContent::kBarType)) {
        return;
    }
    const QString directionKey = QString::fromLatin1(LongpathContent::kDirectionKey);
    if (!entry.config.contains(directionKey)) {
        return;
    }
    const QString bindingKey = QString::fromLatin1(LongpathContent::kBindingKey);
    if (!entry.config.contains(bindingKey)) {
        entry.config.insert(bindingKey, LongpathContent::primaryBinding(entry.typeId, entry.config));
    }
    entry.config.remove(directionKey);
}

struct LabelRename {
    const char* typeId;
    const char* oldLabel;
    const char* newLabel;
};

// The picker's old default label for each renamed type, and its new one
// (ContainerContentRegistry::descriptors and the widget types' own
// registrations). makeEntry copies the label into an entry's name, so an
// entry still carrying the old label was never renamed by the operator.
// SMeterPreset is meter.sMeter's older typeId (catalogType). The control.*
// shortcuts are saved as OTHERBTNS under their own names, so only the
// OTHERBTNS grid's old label is listed.
constexpr LabelRename kLabelRenames[] = {
        {"meter.sMeter", "S-meter bar (additional)", "S-Meter bar"},
        {"SMeterPreset", "S-meter bar (additional)", "S-Meter bar"},
        {"meter.comp", "Comp bar", "Compression bar"},
        {"meter.eq", "Eq bar", "EQ bar"},
        {"meter.cfc", "Cfc bar", "CFC bar"},
        {"meter.cfcGain", "CfcGain bar", "CFC gain bar"},
        {"meter.levelerGain", "LevelerGain bar", "Leveler gain bar"},
        {"meter.alcGain", "AlcGain bar", "ALC gain bar"},
        {"meter.alcGroup", "AlcGroup bar", "ALC group bar"},
        {"meter.agc", "Agc bar", "AGC bar"},
        {"meter.agcGain", "AgcGain bar", "AGC gain bar"},
        {"meter.signal", "Signal bar", "Signal peak bar"},
        {"meter.signalAvg", "SignalAvg bar", "Signal average bar"},
        {"meter.signalMaxBin", "SignalMaxBin bar", "Signal max bin bar"},
        {"meter.adc", "Adc bar", "ADC bar"},
        {"meter.adcMax", "AdcMax bar", "ADC maximum bar"},
        {"meter.pbSnr", "PbSnr bar", "PB SNR bar"},
        {"meter.mic", "Mic", "Mic bar"},
        {"meter.alc", "ALC", "ALC bar"},
        {"sMeter.dial", "S-Meter dial (additional)", "S-Meter dial"},
        {"SOLID", "SolidColour", "Solid color"},
        {"FADECOVER", "FadeCover", "Fade cover"},
        {"HISTORY", "HistoryGraph", "History graph"},
        {"MAGICEYE", "MagicEye", "Magic eye"},
        {"NEEDLESCALEPWR", "NeedleScalePwr", "Power scale"},
        {"SIGNALTEXT", "SignalText", "Signal text"},
        {"TEXTOVERLAY", "TextOverlay", "Text overlay"},
        {"WEBIMAGE", "WebImage", "Web image"},
        {"FILTERDISPLAY", "FilterDisplay", "Filter display"},
        {"VFO", "VfoDisplay", "VFO display"},
        {"CLICKBOX", "ClickBox", "Click box"},
        {"DATAOUT", "DataOut", "Data output"},
        {"BANDBTNS", "BandButton", "Band buttons"},
        {"NEREUSBAND", "NereusBandButton", "Band buttons (panel style)"},
        {"MODEBTNS", "ModeButton", "Mode buttons"},
        {"FILTERBTNS", "FilterButton", "Filter buttons"},
        {"ANTENNABTNS", "AntennaButton", "Antenna buttons"},
        {"TUNESTEPBTNS", "TuneStepButton", "Tune step buttons"},
        {"OTHERBTNS", "OtherButton", "Other buttons"},
        {"VOICERECPLAY", "VoiceRecordPlay", "Voice record and play"},
        {"applet:mod_monitor", "Mod Monitor", "AM Mod Monitor"},
        {"applet:tci", "TCI", "TCI Server"},
        {"applet:tci_clients", "Client Chain", "TCI Clients"},
        {"applet:amp", "Amplifier", "4O3A Power Genius"},
        {"applet:tuner", "Tuner", "4O3A Tuner Genius"},
        {"applet:RfKit", "RF Kit", "RF-Kit RF2K-S"},
        {"longpath.frequency", "Frequency (Longpath)", "Frequency"},
        {"longpath.frequencyCustom", "Frequency (Custom)", "Custom frequency"},
        {"longpath.needle", "Needle meter (Longpath)", "Needle meter"},
        {"longpath.bar", "Bar meter (Longpath)", "Bar meter"},
        {"longpath.filter", "Filter passband (Longpath)", "Filter passband"},
};

void migrateDefaultName(ContentEntry& entry)
{
    for (const LabelRename& rename : kLabelRenames) {
        if (entry.typeId == QLatin1String(rename.typeId) && entry.name == QLatin1String(rename.oldLabel)) {
            entry.name = QString::fromLatin1(rename.newLabel);
            return;
        }
    }
}

} // namespace

void migrateEntryOnLoad(ContentEntry& entry)
{
    migrateLongpathDirection(entry);
    migrateDefaultName(entry);
}

} // namespace NereusSDR
