#pragma once
// =================================================================
// src/core/settings/SettingsScope.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. Remote-daemon R2 Task 14.
//
// Some AppSettings keys describe the station: the radio's DSP
// configuration, which peripherals are attached and how they are
// configured. Others describe the operator's own machine: window
// geometry, trace colours, which local sound card feeds the speakers, and
// (since R-R3-42) the TCI server that serves apps on that machine. classifySettingsKey() is the pure function that decides which
// is which, so Task 15's SettingsProxy has one place to ask before
// deciding whether a write crosses the wire.
//
// A key with no explicit rule below defaults to OperatorLocal, not
// Station. This is deliberate, not an oversight: a client-local key
// wrongly classified Station gets written into the shared station store
// by every client that connects, quietly overwriting one operator's
// window layout with another's; a station key wrongly classified
// OperatorLocal just fails to stick, which is a setting that is
// immediately visibly broken and easy to file a bug against. Both are
// bugs; the second is the recoverable one, so it is the default.
//
// See SettingsScope.cpp for the ordered rule table and its provenance,
// and tests/tst_settings_scope.cpp for the completeness sweep that is the
// actual point of this task: extracting every key literal this tree
// passes to an AppSettings accessor and asserting (a) every key src/core
// or src/models touches classifies Station, modulo a small, justified
// exemption list, and (b) -- the valuable half -- any key that ALSO
// appears in a Setup page (src/gui/setup/) classifies Station. Without
// (b), a misclassified key reads locally, writes locally, sticks in the
// widget, survives a relaunch, and never reaches the station while
// looking like it works.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-06  J.J. Boyd / KG4VCF  Remote daemon R2 Task 14:
//                                    classifySettingsKey and its
//                                    completeness gate. AI-assisted
//                                    transformation via Anthropic Claude
//                                    Code.
//   2026-09-23  J.J. Boyd / KG4VCF  R3 receiver audio plan, Task 4
//                                    (R-R3-42): TCI settings are this
//                                    computer's. AI-assisted
//                                    transformation via Anthropic Claude
//                                    Code.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Task 1 (R-IOS-01):
//                                    settingsScopeRules(), the rule tables
//                                    read out for the link surface.
//                                    Classification unchanged. AI-assisted
//                                    transformation via Anthropic Claude
//                                    Code.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Task 13 (R-IOS-08):
//                                    isCoreOwnedIdentitySettingsKey().
//                                    AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QList>
#include <QString>
#include <QStringView>

namespace NereusSDR {

enum class SettingsScope {
    /// Belongs to the operator's own machine and must never leave the
    /// local AppSettings store: window geometry, trace/grid/waterfall
    /// colours, which local sound card feeds the speakers or mic input,
    /// per-process log verbosity. The default for any key with no more
    /// specific rule -- see this header's top comment for why defaulting
    /// local, not Station, is deliberate.
    OperatorLocal,

    /// Belongs to the radio-owning daemon and must round-trip over the
    /// wire so every GUI connected to the same station sees and controls
    /// the same value: DSP/radio hardware configuration,
    /// peripheral (PGXL/TGXL/RF2K-S) state, per-slice VFO/AGC/
    /// filter/NR state, TX safety interlocks.
    Station,
};

// Classifies key -- an AppSettings top-level key exactly as it appears in
// the flat settings store, i.e. AFTER setHardwareValue()/hardwareValue()
// have already prefixed it with "hardware/<mac>/" (AppSettings.cpp:974,
// 981), not the bare suffix literal a call site passes to those two
// accessors -- as Station or OperatorLocal.
//
// A trailing "_<panIndex>" suffix is stripped before matching (per-pan
// keys use the AetherSDR pattern ported into SpectrumWidget.cpp:577-584's
// settingsKey(base, panIndex) helper: "DisplayFftSize" for pan 0,
// "DisplayFftSize_1" for pan 1), so a key classifies identically on every
// pan.
//
// Pure function: no I/O, no singleton, no dependency on AppSettings
// itself. Links against exactly one .cpp. Task 15's SettingsProxy asks
// this, per key, before deciding whether a write crosses the wire.
SettingsScope classifySettingsKey(QStringView key);
// Core models validate these values; the raw settings proxy may observe them
// but cannot write/remove them around that acceptance boundary.
bool isModelOwnedDspSettingsKey(QStringView key);
// R-R3-21 / R-R3-09: the Core's notch list and its two flags (NotchCount,
// Notch<N>{Center,Width,Active}, NotchGlobalEnabled, NotchAutoIncrease).
// NotchVisualEnabled is not one: it is a plain Station key, one Core-wide
// value every window reads and writes through the settings proxy.
bool isModelOwnedNotchSettingsKey(QStringView key);
// R-R3-46 / R-R3-11: the Core's step attenuator and preamp settings,
// hardware/<mac>/options/{stepAtt,autoAtt,preamp}/... A window changes them
// through the mirrored `stepAtt` object.
bool isModelOwnedStepAttenuatorSettingsKey(QStringView key);
// R-R3-46 (radioHardwareVersion 2): the Core's Alex antenna settings,
// hardware/<mac>/alex/antenna/... AlexController saves them itself (and
// again at teardown), so a raw write would be overwritten; a window changes
// the receive ones through the mirrored `alexAntennas` object.
bool isModelOwnedAlexAntennaSettingsKey(QStringView key);
// iPhone app Task 13 (R-IOS-08): the Core's name (StationLabel) and its
// identity key backup acknowledgement (StationKeyBackupAcknowledged), which
// change only through station.rename and station.acknowledgeKeyBackup.
bool isCoreOwnedIdentitySettingsKey(QStringView key);
// The reason a raw write or remove of a model-owned key is refused, in the
// operator's words.
QString modelOwnedSettingsRefusal(QStringView key);

/// Which of classifySettingsKey()'s three tables a rule comes from. It
/// walks them in this order, after the model-owned keys and after
/// stripping a "_<digits>" pan suffix: whole-key exceptions, then
/// prefixes, then whole keys; a key no rule names is OperatorLocal.
enum class SettingsScopeRuleKind {
    Exception,
    Prefix,
    WholeKey,
};

struct SettingsScopeRule {
    SettingsScopeRuleKind kind = SettingsScopeRuleKind::Prefix;
    QString text;
    SettingsScope scope = SettingsScope::OperatorLocal;
};

/// R-IOS-01: every rule classifySettingsKey() walks, in the order it walks
/// them, so the link surface is read from the tables rather than copied.
QList<SettingsScopeRule> settingsScopeRules();

} // namespace NereusSDR
