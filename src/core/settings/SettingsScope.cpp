// =================================================================
// src/core/settings/SettingsScope.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. Remote-daemon R2 Task 14.
//
// The ordered rule table, first match wins:
//   1. Explicit exceptions -- a key that would otherwise be caught by a
//      prefix rule below but needs the opposite answer. The nine
//      FreeDvReporter/* window-presentation keys (which would match
//      "FreeDv") qualify under that strict definition. The two
//      TciLogWindow* keys are listed there too, unchanged, although since
//      R-R3-42 the "Tci" prefix rule gives them the same answer. A key
//      that matches no prefix at all belongs in step 3 below, even if it
//      reads like an "exception" to some family's usual answer in prose.
//   2. Prefixes -- a whole family that shares one scope regardless of
//      what follows the prefix (checked with startsWith(), so ordering
//      between prefixes only matters if one is a leading substring of
//      another; none of the ones below are).
//   3. Whole-key rules -- individual flat keys with no shared prefix
//      family, including several that are departures from what their
//      surrounding family would otherwise suggest (the four FftPoolConfig
//      knobs among the mostly-cosmetic "Display*" keys; audio/DspRate and
//      audio/DspBlockSize among the mostly-local "audio/*" keys) -- see
//      the two paragraphs below for why those six live here and not in
//      step 1.
//   4. Default: OperatorLocal. See SettingsScope.h's top comment for why
//      the default is local, not Station.
//
// Two families deliberately do NOT get a blanket prefix rule even though
// they look like good prefix candidates, because the same textual prefix
// covers keys with genuinely different scopes:
//
//   "Display" -- most Display* keys are pure client-side rendering
//   (colours, line widths, hold timers for the peak/blob overlay) and are
//   correctly OperatorLocal by the bare default below. The RX FftPoolConfig
//   quartet is not: DisplayFftSize, DisplayFftWindow,
//   DisplayHzPerBinTarget and DisplaySpectrumFps, which
//   MainWindow::refreshFftPoolConfig
//   (MainWindow.cpp:1477-1500, this exact quartet named at :1512-1539's
//   "the four display AppSettings-sourced knobs" comment) reads to
//   configure the daemon's actual FFT production rate/size/window/target
//   bin width. The nine DisplayTx* analyzer settings are also Station:
//   TxAnalyzer::loadSettings() reads them to configure the TX-side WDSP
//   analyzer, and saveSettings() writes the same exact set. These thirteen
//   keys are whole-key rules below, Station, not
//   "explicit exceptions" in this file's strict sense, since no prefix
//   rule claims "Display*" for step 1 to need to override. DisplaySpectrumFps
//   carries its own paragraph (see below) because it is not simply
//   "Station" in the same sense as the other three.
//
//   "audio/" -- almost every audio/* key is local sound hardware
//   selection (AudioEngine.cpp's ensureSpeakersOpen/ensureTxInputOpen via
//   AudioDeviceConfig::loadFromSettings("audio/Speakers"|"audio/TxInput"),
//   VAX cable bookkeeping, the v0.3.0 audio/FirstRunComplete migration
//   flag) and correctly falls through to the default below. audio/DspRate
//   and audio/DspBlockSize are the two departures: real WDSP engine
//   parameters (AudioEngine.cpp:1791-1808, read back by
//   AudioAdvancedPage.cpp:145-170), not device selection, so they are
//   Station whole-key rules below. See tests/tst_settings_scope.cpp's
//   kCoreExemptPrefixes for the fuller "audio/*" writeup, including that
//   src/core/daemon/DaemonApp.cpp:220 also writes audio/Speakers/DeviceName
//   directly (from nereusd.conf's audio_device) without that making it
//   Station either -- it is still local sound hardware selection, just on
//   whichever machine happens to be running nereusd standalone.
//
// ---- The DisplaySpectrumFps straddle -----------------------------------
//
// DisplaySpectrumFps is read TWICE for two different reasons on two sides
// of the R2 split, and a three-valued (well, two-valued) scope enum
// cannot encode "both, meaning different things at each end":
//   - MainWindow.cpp:1483, into FftPoolConfig::fps -- the rate at which
//     the daemon's FFT engines actually produce spectrum frames. This
//     belongs to the station.
//   - MainWindow.cpp:3485, into the CLIENT's own SpectrumWidget paint
//     timer (setDisplayFps) -- how often ONE client's own screen redraws
//     the trace it already has. This belongs to the operator's machine;
//     a remote client with a 165 Hz monitor and a local client on a
//     10-year-old laptop have no reason to share this number.
// This function classifies it Station, grouped with the other three
// FftPoolConfig knobs, because the daemon-side consumer is the one that
// actually changes what data exists (the production rate), while the
// client-side consumer only changes how often an already-produced frame
// is redrawn -- the less consequential of the two if the value is wrong.
// This is a known, accepted, UNRESOLVED straddle, not a fix: a future
// task that wants the client's own paint rate independently configurable
// needs its own key (e.g. a client-local "DisplayPaintFps"), not a second
// meaning for this one.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-29  J.J. Boyd / KG4VCF  Level Cal: RX1_DisplayCalOffsetDb is a
//                                    station key. AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  Level Cal: the per-model meter and
//                                    display calibration keys. AI-assisted
//                                    via Anthropic Claude Code.
//   2026-08-06  J.J. Boyd / KG4VCF  Remote daemon R2 Task 14:
//                                    classifySettingsKey and its
//                                    completeness gate. AI-assisted
//                                    transformation via Anthropic Claude
//                                    Code.
//   2026-08-06  J.J. Boyd / KG4VCF  Fix round 1 (review): moved six
//                                    whole-key rules out of kExceptions
//                                    into kWholeKeys (they match no
//                                    prefix, so they were never really
//                                    "exceptions" by this file's own
//                                    definition); added a "radios/"
//                                    prefix rule (hand-seeded, same
//                                    reason "Slice" is); removed the
//                                    "%1/" prefix rule (a test-only
//                                    extraction artifact that had leaked
//                                    into production code, now handled
//                                    entirely inside the test); corrected
//                                    a false claim in stripPanSuffix's
//                                    comment. AI-assisted transformation
//                                    via Anthropic Claude Code.
//   2026-08-09  J.J. Boyd / KG4VCF  Whole-branch review, Important 3: ten
//                                    FreeDvReporter/* window-presentation
//                                    keys move from Station to explicit
//                                    OperatorLocal exceptions, and the
//                                    "FreeDv" prefix rule's justification
//                                    is corrected to what is actually
//                                    true of it. AI-assisted
//                                    transformation via Anthropic Claude
//                                    Code.
//   2026-09-23  J.J. Boyd / KG4VCF  R-R3-46 / R-R3-11: the Core's step
//                                    attenuator and preamp keys
//                                    (options/stepAtt, options/autoAtt,
//                                    options/preamp) are Core-owned.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-23  J.J. Boyd / KG4VCF  R3 receiver audio plan, Task 4
//                                    (R-R3-42): the "Tci" prefix moves
//                                    from Station to OperatorLocal (the
//                                    TCI server runs on this computer);
//                                    values a Core stored are ignored, not
//                                    migrated. AI-assisted transformation
//                                    via Anthropic Claude Code.
//   2026-09-23  J.J. Boyd / KG4VCF  R3 receiver audio plan, Task 5
//                                    (R-R3-44): the "RemoteVax/" prefix, a
//                                    remote window's VAX channel per Core
//                                    and slice, is OperatorLocal.
//                                    AI-assisted transformation via
//                                    Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Task 1 (R-IOS-01):
//                                    settingsScopeRules(). AI-assisted
//                                    transformation via Anthropic Claude
//                                    Code.
//   2026-09-24 - R-R3-48: StationTci_ is Station scope. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-24 - iPhone app Task 4b (R-IOS-01, R-R3-21): the reasons this
//                file sends an app are in operator words. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-24 - R-R3-49: NetworkWatchdogEnabled is Station scope. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-24 - iPhone app Task 13 (R-IOS-08): StationLabel and
//                StationKeyBackupAcknowledged are Core-owned. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-24 - iPhone app Task 19 (R-IOS-06, D40): the "filters/" prefix,
//                the filter presets, is Station scope. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-25 - iPhone app plan Task 38 (R-IOS-04): the seven transmit
//                time-out keys are Station scope. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-25 - Receiver and transmit gaps plan, Task 16: RxOnly is
//                Station scope. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-26 - Parity Task 19 (R-IOS-25): the WSJT-X and SpotCollector
//                keys are this computer's (OperatorLocal); the Core runs
//                the station's other spot sources. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-27 - iPhone plan Task 22 / parity Task 20 (R-IOS-26): the Core
//                runs FreeDV Reporter, so "Hide my station"
//                (FreeDvReporter/Hidden) is the Core's (Station) and no
//                longer a window exception; the saved status messages
//                (FreeDvReporter/SavedMessages) are recorded as Station.
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-27 - R-IOS-13 / R-R3-49 (txModMonitorVersion 1): the AM Mod
//                Monitor's feedback receiver (ModMon/FbStream) is the
//                Core's (Station): it picks which of the Core's receivers
//                the Core's feedback analyzer listens to. The applet's
//                other ModMon/ keys stay each window's. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-28 - Parity ruling C12: the per-band grid dB max and min
//                (DisplayGridMax_ / DisplayGridMin_) are the Core's
//                (Station). J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                Claude Code.
//   2026-09-28 - R-R3-49 / R-R3-46: Setup > Transmit > Power's Disable HF PA
//                applied (Thetis DisablePA and hf_tr_relay,
//                transmitSettingsVersion 11). J.J. Boyd (KG4VCF), AI-assisted
//                via Anthropic Claude Code.
// =================================================================

#include "core/settings/SettingsScope.h"

#include <QChar>
#include <QLatin1String>
#include <QRegularExpression>

namespace NereusSDR {

namespace {

// From SpectrumWidget.cpp:577-584's settingsKey(base, panIndex) helper
// (AetherSDR pattern): panIndex 0 returns base unchanged; panIndex N>0
// returns "base_N". Strip a trailing "_<digits>" run before matching, so
// a key means the same thing on every pan.
//
// A handful of OTHER index-suffixed key families exist in this tree,
// built via .arg() and therefore invisible to the completeness sweep's
// extraction (see tst_settings_scope.cpp's "Three limits" comment):
// ContainerData_%1, ContainerItems_%1 (ContainerManager.cpp, container
// id, a QUuid string), SpotBandFilter_%1, spotListBandPill_%1 (band name
// like "20m"), SpotSourceFilter_%1, spotListSourcePill_%1 (source label).
// None of these six are misclassified by this function whether or not
// stripping happens to fire on a given instantiation: none of them, base
// or suffixed, matches any rule below, so all six resolve OperatorLocal
// regardless -- correctly, since all six are per-widget/per-filter local
// UI state. This function's correctness does not depend on identifying
// every index-suffixed family in the tree, only on not mis-stripping one
// that WOULD otherwise match a rule; nothing in the current rule table
// shares a base name with any of these six.
QStringView stripPanSuffix(QStringView key)
{
    const qsizetype underscore = key.lastIndexOf(QLatin1Char('_'));
    if (underscore < 0 || underscore == key.size() - 1) {
        return key;
    }
    const QStringView suffix = key.sliced(underscore + 1);
    for (const QChar c : suffix) {
        if (!c.isDigit()) {
            return key;
        }
    }
    return key.first(underscore);
}

struct Rule {
    const char* text;
    SettingsScope scope;
};

// ---- 1. Explicit exceptions ---------------------------------------------
const Rule kExceptions[] = {
    // TCI's own log-viewer dialog (Tools -> ... -> TCI Server Log,
    // TciLogWindow.cpp): pure GUI chrome. These escaped a Station "Tci"
    // prefix rule until R-R3-42 made that rule OperatorLocal too; they stay
    // here unchanged so their answer never depends on the prefix rule.
    { "TciLogWindowGeometry", SettingsScope::OperatorLocal },
    { "TciLogWindowAutoScroll", SettingsScope::OperatorLocal },

    // ---- The FreeDV Reporter window's own presentation state ----------
    //
    // Whole-branch review, Important 3. The "FreeDv" prefix rule below is
    // justified on RadioModel owning the client instance, with the dialogs
    // only presenting what it already collected. That holds for
    // FreeDvReporter/{Callsign,GridSquare,Message,ServerUrl}, which
    // src/models/RadioModel.cpp really does read. It does not hold for the
    // nine below: table geometry, sort state, column visibility, per-column
    // filters and three display-unit toggles, whose
    // only reader or writer anywhere in the tree is
    // src/gui/FreeDVReporterDialog.cpp or src/gui/SpotHubDialog.cpp.
    //
    // Left Station, each of these was over-classified, the direction
    // SettingsScope.h's own default-local rationale names as the worse
    // one: on a remote client, dragging a column divider wrote into the
    // proxy cache and out onto the wire, the value never reached the
    // operator's own settings file, reads were shadowed by the proxy so
    // widths saved during earlier LOCAL use were ignored while remote, and
    // the daemon's store accumulated one GUI's table geometry for every
    // client that ever connected.
    //
    // These are true "explicit exceptions" in this file's strict sense:
    // every one is caught by the "FreeDv" prefix rule in step 2 without
    // this step running first.
    //
    // Three further FreeDvReporter/* keys are also written only from
    // src/gui and are deliberately NOT moved here, because each is a
    // judgement rather than a presentation fact and none was in the
    // review's scope: SavedMessages (the operator's canned status-message
    // presets, whose SENT counterpart Message is Station; iPhone plan Task
    // 22 confirms Station: every device offers the Core's saved messages),
    // ReportToPsk (a
    // reporting BEHAVIOUR flag that has no runtime consumer at all today,
    // the same shape as the five reviewed-and-pinned entries at the bottom
    // of kWholeKeys), and IdleTimeoutMinutes (the dialog's own idle-sweep
    // threshold, and the only expiry FreeDVStationModel has anywhere).
    // Recorded so the next reader sees they were checked, not missed.
    { "FreeDvReporter/ColumnWidths", SettingsScope::OperatorLocal },
    { "FreeDvReporter/SortColumn", SettingsScope::OperatorLocal },
    { "FreeDvReporter/SortAscending", SettingsScope::OperatorLocal },
    { "FreeDvReporter/VisibleColumns", SettingsScope::OperatorLocal },
    { "FreeDvReporter/ColumnFilters", SettingsScope::OperatorLocal },
    { "FreeDvReporter/BandFilter", SettingsScope::OperatorLocal },
    // FreeDvReporter/Hidden left this list with iPhone plan Task 22: the
    // Core runs FreeDV Reporter, so "Hide my station" is the Core's
    // (freedv.setHidden) and the "FreeDv" prefix rule makes it Station.
    { "FreeDvReporter/DistanceMiles", SettingsScope::OperatorLocal },
    { "FreeDvReporter/DirectionAsCardinal", SettingsScope::OperatorLocal },
    { "FreeDvReporter/FrequencyAsKhz", SettingsScope::OperatorLocal },
};

// ---- 2. Prefixes ---------------------------------------------------------
const Rule kPrefixes[] = {
    // Every hardware/<mac-or-literal-segment>/* key, without exception.
    // AppSettings::hardwareValue()/setHardwareValue() (AppSettings.cpp:
    // 974, :981) always prepend exactly "hardware/%1/".arg(mac) before a
    // key reaches setValue()/value(), so every key that ever reaches
    // Task 15's change hook through that pair already carries this
    // prefix -- radioInfo/*, options/{stepAtt,autoAtt,preamp}/*,
    // p1AdcCntrl, peripherals/*, alex/{hpf,lpf,bpf1}/*, alex2/*, apollo
    // state, PureSignal autoCalEnabled, PA/mic profile storage, all of
    // it. A handful of call sites build the same "hardware/%1/..." shape
    // by hand via plain setValue()/value() instead of going through that
    // pair (AlexController.cpp/ApolloController.cpp's own
    // persistenceKey() -> "hardware/<mac>/alex/antenna" or
    // "hardware/<mac>/apollo"; SettingsHygiene.cpp; CalibrationController
    // .cpp; MicProfileManager.cpp; PaProfileManager.cpp; OcMatrix.cpp;
    // TxSliceArbiter.cpp) -- same prefix, same scope, no separate rule
    // needed. hardware/oc/pennyExtCtrl (OcOutputsHfTab.cpp:373) is the
    // brief's own canonical proof that the segment after "hardware/" is
    // just whatever text a call site put there, MAC or literal "oc" or
    // anything else -- this rule does not, and must not, try to validate
    // that it looks like a MAC.
    { "hardware/", SettingsScope::Station },

    // TCI server configuration (CatTciServerPage / AudioTciPage /
    // TciServer / TciProtocol / TciApplet): server bind address/port/
    // enabled, ExpertSDR3/SunSDR2Pro/CWLU compatibility flags, IQ/audio
    // stream shape, sensor poll intervals, rate limiting, the TCI gain
    // sliders. R-R3-42: all of it configures the TCI WebSocket server,
    // which runs in the window on this computer and serves apps on this
    // computer. (R-R3-48: the Core also runs one on the station network;
    // its switch and port are the Core's own StationTci_ keys below, and
    // it reads no Tci key but the compatibility defaults.) A
    // remote window's TCI settings are therefore its own, like a local
    // window's; Tci values a Core stored while this rule said Station are
    // ignored, not migrated. The rule is explicit rather than left to the
    // default so the family's answer is written down in one place.
    { "Tci", SettingsScope::OperatorLocal },

    // Per-slice-per-band DSP/VFO state (AppSettings.h's documented
    // "Slice<N>/Band<key>/..." and "Slice<N>/..." families: AGC, filter,
    // NR stack, RIT/XIT, AF/RF gain, antennas, mode, step). Not
    // mechanically discovered by the completeness sweep below --
    // SliceModel.cpp builds every key through bandPrefix()/slicePrefix()
    // helpers (SliceModel.cpp:1740, :1759, :1766, :1768: both literally
    // start with QStringLiteral("Slice%1/...")) and then concatenates a
    // field name onto the result, so no call site passes a literal
    // starting with "Slice" directly to value()/setValue(). Hand-seeded
    // for the same reason the hardwareValue reconstruction below exists:
    // the REAL key Task 15 will see always has this prefix, even though
    // no single source line spells it out. This is the single most
    // important prefix in this table -- it is the live VFO/AGC/filter/NR
    // state that R2 exists to keep in sync.
    { "Slice", SettingsScope::Station },

    // Legacy flat "Vfo*" keys, one-shot migrated into the Slice<N>/...
    // namespace above by SliceModel::migrateLegacyKeys() on startup
    // (AppSettings.h's doc comment on that namespace). Same content,
    // same scope as their successor.
    { "Vfo", SettingsScope::Station },

    // Saved-radio list (name/IP/MAC/port/protocol/autoConnect/pinToMac/
    // lastSeen/lastConnected/discoveryProfile). OperatorLocal, not
    // Station: which radios are known/discoverable and which to
    // auto-connect to is a client-side connection-management concern,
    // not something nereusd needs. Confirmed by absence, not presence:
    // grepping src/core/daemon/DaemonApp.cpp and DaemonConfig.cpp for
    // "radios/"/savedRadio/lastConnected/discoveryProfile returns zero
    // hits -- the daemon picks its target radio from nereusd.conf, never
    // from this namespace. Hand-seeded like "Slice" above: every real
    // call site is inside AppSettings.cpp's own lastConnected()/
    // setLastConnected()/discoveryProfile()/setDiscoveryProfile() (and
    // the radioKeyPrefix()/savedRadio()/saveRadio() family), calling
    // value()/setValue()/remove() with no receiver prefix at all
    // (implicit self-calls), which is outside what the completeness
    // sweep's regex can see (see tst_settings_scope.cpp's "Three limits"
    // comment) -- so nothing forces this rule to exist for either
    // completeness assertion to pass; it is here because it is correct,
    // not because a test demanded it. Explicit rather than left to the
    // bare default so a future reader sees this was reviewed, not missed.
    { "radios/", SettingsScope::OperatorLocal },

    // R-R3-38: client target selection and each Core's trust tuple never
    // belong to the selected station's settings snapshot.
    { "ConnectionTargets/", SettingsScope::OperatorLocal },

    // R-R3-44: which of this computer's VAX channels a remote window gives
    // each of a Core's slices (RemoteVaxRouter::settingsKey,
    // "RemoteVax/<core>/Slice<N>/Channel"). This computer's VAX, kept per
    // Core; never the Core's own Slice<N>/VaxChannel.
    { "RemoteVax/", SettingsScope::OperatorLocal },

    // PGXL / TGXL / RF2K-S: physically attached to the station (the
    // amplifier/tuner sits at the radio site, not on an operator's
    // remote laptop). Connection config, pairing state, identity/nickname,
    // antenna labels (describing what is physically plugged into THIS
    // station's ports), fault history, tune memory, interlock policy.
    { "PGXL_", SettingsScope::Station },
    { "TGXL_", SettingsScope::Station },
    { "RfKit_", SettingsScope::Station },
    // R-R3-48: the Core's station TCI switch and port, written by the Core
    // itself when a window's TCI switch sends setStationTci.
    { "StationTci_", SettingsScope::Station },

    // Spot-source client connection + display config (DX cluster/RBN
    // telnet, WSJT-X UDP, SpotCollector/DXLab UDP, POTA HTTPS, FreeDV
    // Reporter Socket.IO, PSK Reporter IPFIX). RadioModel.h/.cpp owns
    // every one of these six client instances directly, so in the R2
    // split they run in the daemon. Covers both the connection half
    // (host/port/poll-interval) and the per-source display half (spot
    // marker colour/lifetime): both are read by src/models/RadioModel.cpp
    // today, and nothing in this tree currently splits "which server"
    // from "what colour its dots are" into two different scopes for the
    // same source.
    //
    // Whole-branch review, Important 3 corrected the reasoning above. It
    // used to say the dialogs "only present what RadioModel already
    // collected" and that RadioModel owns EVERY key in these families.
    // The first half is true of the collected SPOTS; the second is false
    // of the FreeDV Reporter window's own table geometry, sort state,
    // column visibility, view filters and display-unit toggles, which
    // RadioModel never reads and which are now nine explicit exceptions in
    // step 1 above. What survives, and is what actually justifies these
    // prefix rules, is narrower: the CONNECTION half and the SPOT-DISPLAY
    // half of each source are read by src/models/RadioModel.cpp. A key
    // under one of these prefixes that no core or model consumer reads is
    // not automatically Station, and the exceptions list is where it
    // belongs.
    { "DxCluster", SettingsScope::Station },
    { "Rbn", SettingsScope::Station },
    { "Pota", SettingsScope::Station },
    { "PskReporter", SettingsScope::Station },
    { "FreeDv", SettingsScope::Station }, // FreeDvReporter/* and FreeDvSpot*
    // Parity Task 19 (R-IOS-25; remote design section 6.4): WSJT-X and
    // SpotCollector listen for programs on the computer they run on, so
    // each window runs its own and keeps its own settings (addresses,
    // ports, colours, lifetimes). The Core runs the four sources above.
    { "SpotCollector", SettingsScope::OperatorLocal },
    { "Wsjtx", SettingsScope::OperatorLocal },

    // Self-reporting identity used to seed the spot-source clients above
    // (POTA/PSKReporter/FreeDVReporter self-spots, distance/bearing
    // calculations against other stations). Grid square in particular
    // must be the STATION's grid for those calculations to be correct in
    // a genuinely remote session, not the operator's own location.
    { "User/", SettingsScope::Station },

    // TNF (tunable notch filter): an actual WDSP notch in the RX audio
    // chain, not a display annotation. NotchModel state
    // (NotchGlobalEnabled/NotchVisualEnabled/NotchAutoIncrease/
    // NotchCount/Notch<N>{Active,Center,Width}). Since R-R3-21 all but
    // NotchVisualEnabled are also model-owned (isModelOwnedNotchSettingsKey
    // below): the Core changes its list only through notch.* commands.
    { "Notch", SettingsScope::Station },

    // Setup -> DSP page (DspOptionsPage.cpp): WDSP buffer/filter size,
    // impulse-response cache. Engine configuration, not a display
    // preference.
    { "DspOptions", SettingsScope::Station },

    // Noise blanker (NB/NB2) and sub-band noise blanker (SNB) DEFAULT
    // tuning, applied when a slice/band has no per-band override yet.
    // WDSP parameters (cmaster.c defaults per AppSettings.h's own
    // per-slice-per-band doc comment).
    { "Nb", SettingsScope::Station }, // NbDefault*, Nb2DefaultMode
    { "Snb", SettingsScope::Station }, // SnbDefaultK1/K2/OutputBW

    // RADE peer-mode DSP: neural vocoder model path and the EOO
    // idle-clear timer, both configuring the daemon-side RadeChannel.
    { "Rade", SettingsScope::Station }, // Rade/ModelPath, RadeIdleClearMs

    // iPhone app Task 19 (D40): the filter presets
    // (FilterPresetStore's filters/<mode>/<slot>/{name,low,high}) live on
    // the Core, like the rest of a slice's filter state, so every window
    // and device connected to it shows and edits the same presets, and the
    // Core's catalogue sends them to an app. A window running its radio
    // locally has no remote backend, so nothing changes for it.
    { "filters/", SettingsScope::Station },

    // Parity ruling C12 (remote-window parity plan, 2026-09-24): Setup >
    // Display > Grid & Scales' per-band dB max and min
    // (PanadapterModel's DisplayGridMax_<band> / DisplayGridMin_<band>)
    // live on the Core. The Core's pan applies them on a band crossing and
    // its range reaches every window, so every window shows and edits the
    // Core's values. The rest of the Display* family stays local.
    { "DisplayGridMax_", SettingsScope::Station },
    { "DisplayGridMin_", SettingsScope::Station },
};

// ---- 3. Whole-key rules ---------------------------------------------------
const Rule kWholeKeys[] = {
    // The four FftPoolConfig knobs -- see this file's header comment for
    // the full "Display" writeup and the DisplaySpectrumFps straddle
    // paragraph. Not "explicit exceptions" (step 1): no "Display" prefix
    // rule exists for these to escape.
    { "DisplayFftSize", SettingsScope::Station },
    { "DisplayFftWindow", SettingsScope::Station },
    { "DisplayHzPerBinTarget", SettingsScope::Station },
    { "DisplaySpectrumFps", SettingsScope::Station },

    // TX-side WDSP analyzer configuration. TxAnalyzer owns the detector,
    // averaging, normalization, FFT geometry and window applied before TX
    // pixels leave the station, so these must follow the daemon rather than
    // an operator's local display preferences. Keep these exact instead of
    // claiming the broader DisplayTx family.
    { "DisplayTxFftSize", SettingsScope::Station },
    { "DisplayTxWindowType", SettingsScope::Station },
    { "DisplayTxPanDetector", SettingsScope::Station },
    { "DisplayTxPanAveraging", SettingsScope::Station },
    { "DisplayTxPanAvTimeMs", SettingsScope::Station },
    { "DisplayTxPanNormalize", SettingsScope::Station },
    { "DisplayTxWfDetector", SettingsScope::Station },
    { "DisplayTxWfAveraging", SettingsScope::Station },
    { "DisplayTxWfAvTimeMs", SettingsScope::Station },

    // The "audio/" split -- see this file's header comment. Both are read
    // by AudioAdvancedPage.cpp:145-170 (Setup -> Audio -> Advanced) and
    // written by AudioEngine.cpp:1791-1808 (setDspSampleRate/
    // setDspBlockSize). Not "explicit exceptions" either: no "audio/"
    // prefix rule exists for these to escape.
    { "audio/DspRate", SettingsScope::Station },
    { "audio/DspBlockSize", SettingsScope::Station },

    // Band-plan / TX-legality is a fact about where the RADIO is, not
    // where the operator's remote GUI happens to be sitting -- the
    // clearest possible case for Station in a genuinely remote session.
    { "BandPlanName", SettingsScope::Station },
    { "BandPlanRegion", SettingsScope::Station },
    // JJ's ruling (2026-09-28, addendum G-42): Extended transmit is one
    // setting of the Core's, default off, read by the Core's transmit gate
    // (RadioModel::installBandPlanMoxCheck). A new key on purpose: an old
    // saved ExtendedTxAllowed (below, still OperatorLocal) is never read,
    // so no computer's or Core's old tick can turn Extended on; the
    // operator ticks it again. Written only with transmit permission and
    // never while the radio is on the air (StationServer).
    { "ExtendedTransmit", SettingsScope::Station },
    // JJ's ruling (2026-09-29): Prevent transmitting on a different band
    // is one setting of the Core's, default off, read by the Core's
    // transmit gate. It keeps Thetis's key name, so a value the Core
    // already saved carries forward; a remote computer's own old value is
    // no longer read (reads go to the Core). It only restricts transmit.
    // Written only with transmit permission and never while the radio is
    // on the air (StationServer).
    { "PreventTxOnDifferentBandToRx", SettingsScope::Station },
    { "Region", SettingsScope::Station }, // GeneralOptionsPage.cpp's FRS
                                            // region combo, same reasoning.

    // CW sidetone/RX-filter pitch offset -- RxChannel.cpp:1208's
    // "Freq = CWPitch + tuneOffset" is a DSP tuning computation.
    { "CWPitch", SettingsScope::Station },

    // Neural-net noise-reduction model file path (NR3/rnnoise). The
    // daemon is the process that actually loads and runs the model. Since
    // R-R3-21 it is also model-owned (isModelOwnedDspSettingsKey below):
    // read only once, to import an older install's model file.
    { "Nr3ModelPath", SettingsScope::Station },

    // Identifies the STATION for self-spotting (POTA/PSKReporter/
    // FreeDVReporter), same family as the "User/" prefix above.
    { "StationCallsign", SettingsScope::Station },

    // Per-model factory S-meter calibration override
    // (HpsdrModel.h/RadioModel.cpp:3087-3101) -- a hardware calibration
    // constant for the connected board, same kind of fact as anything
    // under hardware/, just predating that convention.
    { "RX1_MeterCalOffsetDb", SettingsScope::Station },

    // Level Cal: the RX1 display calibration (RadioModel::
    // rxDisplayCalOffsetDb), the same kind of constant as the meter's.
    { "RX1_DisplayCalOffsetDb", SettingsScope::Station },

    // Level Cal: the meter and display calibrations kept per radio model,
    // as Thetis's rx_meter_cal_offset_by_radio and
    // rx_display_cal_offset_by_radio. The two keys above are the one-value
    // form of earlier builds; a Core moves them here for the connected model.
    { "RxMeterCalOffsetDbByRadio", SettingsScope::Station },
    { "RxDisplayCalOffsetDbByRadio", SettingsScope::Station },

    // Level Cal: the ten preamp settings' receive offsets that the level
    // calibration measures (RadioModel::rx1PreampOffsetDbFor).
    { "RX1_PreampOffsetsDb", SettingsScope::Station },

    // One-shot migration marker for legacy global peripherals/* keys
    // moving to per-MAC hardware/<mac>/peripherals/* scope
    // (RadioModel.cpp:2956, :2992). Paired with data that is itself
    // Station (hardware/ prefix), so the flag travels with it: a fresh
    // GUI client connecting to an already-migrated daemon must not
    // independently re-run a migration against data it does not own.
    { "PeripheralsMigrationDone", SettingsScope::Station },

    // TX safety interlocks -- SwrProtectionController / TxInhibitMonitor
    // / the tune-power / wind-back-on-high-SWR gates
    // (src/core/safety/). Enforcement lives wherever TX actually
    // happens, which in R2 is the daemon.
    { "SwrProtectionEnabled", SettingsScope::Station },
    { "SwrProtectionLimit", SettingsScope::Station },
    { "SwrTuneProtectionEnabled", SettingsScope::Station },
    { "TunePowerSwrIgnore", SettingsScope::Station },
    { "TxInhibitMonitorEnabled", SettingsScope::Station },
    { "TxInhibitMonitorReversed", SettingsScope::Station },
    { "WindBackPowerSwr", SettingsScope::Station },
    // Setup > Transmit > Power's "Disable HF PA": the radio's DisablePA bit
    // and the SWR protection's pass, applied where the radio is
    // (RadioModel::applyDisableHfPaSetting). Until transmitSettingsVersion
    // 11 it was pinned OperatorLocal below as a write-only setting.
    { "DisableHfPa", SettingsScope::Station },

    // MeterPoller's S-meter sample cadence (MultimeterPage.cpp: Thetis
    // udDisplayMeterDelay default 100 ms) -- how often the daemon
    // actually samples the WDSP meter, not a rendering choice.
    { "MultimeterDelayMs", SettingsScope::Station },

    // R-R3-49: the Network Watchdog is a radio setting, applied where the
    // radio is: by the window on a local radio, by the Core on the Core's.
    // RadioModel applies it at connect and on change. Until R-R3-49 it was
    // pinned OperatorLocal below as a write-only setting with no consumer.
    { "NetworkWatchdogEnabled", SettingsScope::Station },

    // iPhone app plan Task 38 (R-IOS-04, D29): the transmit time-out stops
    // the radio where it is, so every limit is the Core's: Thetis's MOX and
    // ping time-outs (the station and computers) and the one for phones
    // and tablets. RadioModel reads them at every tick of the time-out.
    { "MoxTimeOutEnabled", SettingsScope::Station },
    { "MoxTimeOutSeconds", SettingsScope::Station },
    { "PingTimeOutEnabled", SettingsScope::Station },
    { "PingTimeOutSeconds", SettingsScope::Station },
    { "PingTimeOutHost", SettingsScope::Station },
    { "RemoteMoxTimeOutEnabled", SettingsScope::Station },
    { "RemoteMoxTimeOutSeconds", SettingsScope::Station },
    // Task 16 (receiver and transmit gaps plan): Receive Only stops every
    // key on the radio, so it is the radio's setting, applied where the
    // radio is (MoxController's gate, through RadioModel::applyRxOnly).
    // One value for the Core and every window on it: a window cannot hold
    // a radio in receive only that another window then keys. Until Task 16
    // it was pinned OperatorLocal below as a write-only setting.
    { "RxOnly", SettingsScope::Station },

    // R-IOS-13 / R-R3-49 (txModMonitorVersion 1): the AM Mod Monitor's
    // feedback receiver, the receiver stream carrying PureSignal feedback
    // (HL2: rx1), which the Core's feedback analyzer listens to. A fact
    // about the Core's radio, applied where the radio is
    // (RadioModel::applyModMonitorSetting). Only this key: the applet's
    // source, flasher thresholds and meter style (ModMon/Source,
    // PosFlashPct, NegFlashPct, VintageMeters) are each window's.
    { "ModMon/FbStream", SettingsScope::Station },

    // ---- Reviewed and deliberately pinned OperatorLocal --------------
    // Each of these has a name that reads as a TX-safety or station-
    // behaviour flag, which is exactly the shape of key this table
    // exists to get right -- and each was checked, not guessed. Fix
    // round 1 (review) confirmed this narrower and stronger than
    // originally claimed: grepping the quoted literal for each of the
    // five (two since R-R3-49 wired NetworkWatchdogEnabled, Task 16
    // wired RxOnly and transmitSettingsVersion 11 wired DisableHfPa, above)
    // across src/core and src/models (not just the same-named
    // identifier -- an earlier pass's cruder grep matched things like
    // HPSDRHW::HermesLiteRxOnly, BoardCapabilities::isRxOnlySku, and the
    // Alex.cs-ported RxOnlyAnt[] family, none of which are this setting)
    // finds zero AppSettings accessor call sites anywhere outside the
    // Setup page that defines each one (TransmitSetupPages.cpp /
    // GeneralOptionsPage.cpp): all of them are write-only settings with no
    // runtime consumer at all, not settings with a consumer this table
    // just doesn't happen to route to the daemon. Pinned explicitly,
    // rather than left to the bare default below, so that a future
    // audit that DOES wire one of these into real enforcement trips over
    // an explicit line to change instead of a silent default.
    // Superseded by the Core's ExtendedTransmit (above). Kept pinned so a
    // saved value from an older version stays on the computer that saved
    // it and is ignored: it never turns Extended transmit on.
    { "ExtendedTxAllowed", SettingsScope::OperatorLocal },
};

} // namespace

bool isModelOwnedNotchSettingsKey(QStringView rawKey)
{
    // Exactly the keys NotchModel::saveToSettings writes for the list and
    // its two flags: NotchCount, Notch<N>Center|Width|Active (N written
    // without leading zeros), NotchGlobalEnabled and NotchAutoIncrease.
    // NotchVisualEnabled is not model-owned: a plain Station key (the
    // "Notch" prefix rule), one Core-wide value any window may write.
    static const QRegularExpression kNotchKey(
        QStringLiteral("^notch(?:count|globalenabled|autoincrease"
                       "|(?:0|[1-9][0-9]*)(?:center|width|active))$"),
        QRegularExpression::CaseInsensitiveOption);
    return kNotchKey.match(rawKey).hasMatch();
}

bool isModelOwnedStepAttenuatorSettingsKey(QStringView rawKey)
{
    // hardware/<mac>/options/{stepAtt,autoAtt,preamp}/..., every key
    // StepAttenuatorController::saveSettings writes. The Core's controller
    // saves them itself (debounced, and again at teardown), so a raw write
    // would be overwritten; the `stepAtt` object is the way to change them.
    const QString key = rawKey.toString().toLower();
    if (!key.startsWith(QStringLiteral("hardware/"))) {
        return false;
    }
    const QStringList parts = key.split(QLatin1Char('/'));
    return parts.size() >= 5 && parts[2] == QStringLiteral("options")
        && (parts[3] == QStringLiteral("stepatt") || parts[3] == QStringLiteral("autoatt")
            || parts[3] == QStringLiteral("preamp"));
}

bool isModelOwnedAlexAntennaSettingsKey(QStringView rawKey)
{
    // hardware/<mac>/alex/antenna/..., every key AlexController::save
    // writes (per-band TX, RX and RX-only antennas, the Block-TX and relay
    // switches, the per-ADC filter mode).
    const QString key = rawKey.toString().toLower();
    if (!key.startsWith(QStringLiteral("hardware/"))) {
        return false;
    }
    const QStringList parts = key.split(QLatin1Char('/'));
    return parts.size() >= 5 && parts[2] == QStringLiteral("alex")
        && parts[3] == QStringLiteral("antenna");
}

bool isCoreOwnedIdentitySettingsKey(QStringView rawKey)
{
    // StationLabel (StationLabel::kSettingsKey) changes only through
    // station.rename, StationKeyBackupAcknowledged only through
    // station.acknowledgeKeyBackup (StationDevicesFacade).
    return rawKey.compare(QLatin1String("StationLabel"), Qt::CaseInsensitive) == 0
        || rawKey.compare(QLatin1String("StationKeyBackupAcknowledged"), Qt::CaseInsensitive)
               == 0;
}

bool isModelOwnedDspSettingsKey(QStringView rawKey)
{
    const QString key = rawKey.toString().toLower();
    if (key.startsWith(QStringLiteral("dspassets/"))) {
        return true;
    }
    // iPhone app Task 13 (R-IOS-08): the Core's name and its key backup
    // acknowledgement change only through their commands.
    if (isCoreOwnedIdentitySettingsKey(rawKey)) {
        return true;
    }
    // R-R3-21 / R-R3-09: the Core owns the notch list. An older app's
    // whole-list rewrite would replace every notch the Core holds.
    if (isModelOwnedNotchSettingsKey(rawKey)) {
        return true;
    }
    // R-R3-46 / R-R3-11: the Core owns its step attenuator and preamp.
    if (isModelOwnedStepAttenuatorSettingsKey(rawKey)) {
        return true;
    }
    // R-R3-46: the Core owns its Alex antenna settings.
    if (isModelOwnedAlexAntennaSettingsKey(rawKey)) {
        return true;
    }
    // R-R3-21: the Core picks its NR3 model from its own asset store. An
    // older app's raw path would name a file on the app's computer.
    if (key == QStringLiteral("nr3modelpath")) {
        return true;
    }
    if (!key.startsWith(QStringLiteral("hardware/"))) {
        return false;
    }
    const QStringList parts = key.split(QLatin1Char('/'));
    return (parts.size() >= 4 && parts[2] == QStringLiteral("puresignal"))
        || (parts.size() >= 6 && parts[2] == QStringLiteral("slices")
            && parts[4] == QStringLiteral("nnr"));
}

QList<SettingsScopeRule> settingsScopeRules()
{
    QList<SettingsScopeRule> rules;
    for (const Rule& r : kExceptions) {
        rules.append({SettingsScopeRuleKind::Exception, QString::fromLatin1(r.text), r.scope});
    }
    for (const Rule& r : kPrefixes) {
        rules.append({SettingsScopeRuleKind::Prefix, QString::fromLatin1(r.text), r.scope});
    }
    for (const Rule& r : kWholeKeys) {
        rules.append({SettingsScopeRuleKind::WholeKey, QString::fromLatin1(r.text), r.scope});
    }
    return rules;
}

QString modelOwnedSettingsRefusal(QStringView rawKey)
{
    if (rawKey.compare(QLatin1String("Nr3ModelPath"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("This Core keeps its own NR3 models. Update this app to choose one.");
    }
    if (isModelOwnedNotchSettingsKey(rawKey)) {
        return QStringLiteral("This Core keeps its own notch list. Update this app to change notches.");
    }
    if (isModelOwnedStepAttenuatorSettingsKey(rawKey)) {
        return QStringLiteral("This Core keeps its own attenuator and preamp settings. "
                              "Update this app to change them.");
    }
    if (isModelOwnedAlexAntennaSettingsKey(rawKey)) {
        return QStringLiteral("This Core keeps its own antenna settings. "
                              "Update this app to change them.");
    }
    if (rawKey.compare(QLatin1String("StationLabel"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("This Core keeps its own name. Update this app to rename it.");
    }
    return QStringLiteral("The Core changes these settings only through their own controls.");
}

SettingsScope classifySettingsKey(QStringView rawKey)
{
    if (isModelOwnedDspSettingsKey(rawKey)) {
        return SettingsScope::Station;
    }
    const QStringView key = stripPanSuffix(rawKey);

    for (const Rule& r : kExceptions) {
        if (key == QLatin1String(r.text)) {
            return r.scope;
        }
    }
    for (const Rule& r : kPrefixes) {
        if (key.startsWith(QLatin1String(r.text))) {
            return r.scope;
        }
    }
    for (const Rule& r : kWholeKeys) {
        if (key == QLatin1String(r.text)) {
            return r.scope;
        }
    }
    return SettingsScope::OperatorLocal;
}

} // namespace NereusSDR
