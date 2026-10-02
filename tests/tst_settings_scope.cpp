// Remote Daemon R2, Task 14 -- classifySettingsKey and its completeness
// gate.
//
// The design's own risk register names the most likely half-working
// outcome of the whole R2 settings epic: SettingsProxy (Task 15) passes
// every test it ships with and never actually carries a Setup page,
// because a key silently classified OperatorLocal when it should be
// Station reads locally, writes locally, sticks in the widget, survives a
// relaunch, and never reaches the station -- and looks, to every test
// that only checks the happy path, like it works. The completeness sweep
// below is the thing that catches that failure mode, and is the more
// important half of this file.
//
// ---- Table-driven examples (knownExamples) --------------------------
//
// Pins the brief's own worked examples plus every FftPoolConfig key
// (MainWindow.cpp:1512's "the four display AppSettings-sourced knobs"):
// DisplayFftSize / DisplayFftWindow / DisplayHzPerBinTarget /
// DisplaySpectrumFps are Station because MainWindow::refreshFftPoolConfig
// (MainWindow.cpp:1477-1500) reads them to configure the daemon's actual
// FFT production. Only DisplayFftSize is in the brief verbatim; the other
// three are NOT swept by the completeness test below (MainWindow.cpp
// lives outside all three scanned trees -- see completenessSweep()'s
// header comment), so a regression on any of them would otherwise be
// invisible to this file. Also pins hardware/oc/pennyExtCtrl (Station,
// proving "oc" is a literal path segment and not a MAC-shaped guard),
// radios/lastConnected (OperatorLocal, invisible to the sweep for a
// different reason -- see "Three limits" below), and two of this task's
// own exemption-list judgement calls (audio/DspRate Station vs
// audio/Speakers/DeviceName OperatorLocal; both are core-touched, and
// only one of them is a DSP engine parameter -- see completenessSweep()'s
// kCoreExemptPrefixes for the full reasoning).
//
// ---- Per-pan suffix (perPanSuffixIsStrippedBeforeMatching) -----------
//
// SpectrumWidget.cpp:577-584's settingsKey(base, panIndex) helper appends
// "_<panIndex>" for pan 1+ and returns base unchanged for pan 0. Proves
// the suffix is stripped (pan 1, pan 12) AND that stripping requires an
// all-digit suffix, using a real key (TciSliceA_OutputSampleRate) whose
// only underscore is followed by letters, not digits, as the negative
// case -- a naive "strip after the last underscore" implementation would
// mangle this one into "TciSliceA", losing "OutputSampleRate" and (by
// coincidence, since "Tci" is a prefix rule) still passing this
// particular assertion for the wrong reason, which is why the assertion
// below checks equality against the UNSTRIPPED literal instead of just
// checking the resulting scope.
//
// ---- Completeness sweep (completenessSweep) ---------------------------
//
// Extracts every key literal this tree passes directly to an AppSettings
// accessor (value/setValue/contains/remove/stationValue/setStationValue/
// hardwareValue/setHardwareValue) from src/core, src/models AND
// src/gui/setup/ -- recursively. The brief cites the OC keys as living in
// src/gui/setup/OcOutputsHfTab.cpp; that file does not exist at that
// path, it is src/gui/setup/hardware/OcOutputsHfTab.cpp, one directory
// deeper. Measured: src/gui/setup/ has 32 .cpp files at depth 1 (and 32
// .h files there too, 64 combined) versus 47 recursive .cpp (94 .h+.cpp
// combined) -- a non-recursive scan would silently miss 15 .cpp files
// (23 keys, including every OC key), which is the exact family Step 1
// above hand-pins as a regression canary for this. scanTree()'s
// QDirIterator glob below counts both .h and .cpp, so the failure this
// scan needs to detect is the 64-file depth-1 total, not the 32-file
// .cpp-only figure the brief quotes -- fix round 1 (review) caught an
// earlier draft of this file's minimum-files-scanned guard checking a
// threshold (40) between those two numbers, which is satisfied by the
// non-recursive 64-file trap and therefore could not detect the one
// regression it exists to catch; see the guard's own comment below for
// the corrected floor and the reasoning.
//
// Then asserts two things:
//   (a) every key found in src/core or src/models classifies Station,
//       except the small, individually-justified exemption list (mostly
//       "audio/*": local sound hardware selection genuinely is a
//       per-machine concern even though core/AudioEngine.cpp is the code
//       that opens the device, on whichever machine happens to be
//       running it -- see kCoreExemptPrefixes/kCoreExemptExact below for
//       every entry and its citation);
//   (b) -- the valuable half -- any key found in BOTH src/gui/setup/ and
//       (src/core or src/models) classifies Station. No exemption list
//       here on purpose: a key a Setup page writes into the same
//       namespace a core consumer reads is exactly the shape of the
//       failure this test exists to catch.
//
// Fix round 1 (review) correction: (a) and (b) are not independent the
// way that framing suggests. Both range over coreModelsKeys, (b) via
// coreModelsKeys intersected with setup.keys; QVERIFY2 returns from the
// slot on failure, so (a) always runs to completion first, and (b) is
// only ever REACHED for a key (a) did not already reject -- which means
// (b) can only independently catch something for a key that is BOTH in
// the overlap AND in the exemption list (isCoreExempt), since that is
// the only way (a) lets a key through without demanding Station from it.
// Measured live population of that set, as shipped: exactly TWO keys,
// audio/DspRate and audio/DspBlockSize (both currently classify Station
// via explicit whole-key rules, so (b) does not actually fail for either
// today). Every other member of the 27-key overlap set is non-exempt, so
// a misclassification there is caught by (a) first and (b) never
// executes for it. This does not make (b) pointless -- it is exactly the
// check that would catch a FUTURE exemption accidentally covering a key
// a Setup page also writes -- but its current reach is narrower than
// "any overlapping key" and a reader should not assume otherwise. See
// the sabotage-and-revert proof in the task report for a transcript
// isolating (b)'s own failure message using exactly this two-key
// population (audio/DspRate).
#include <QtTest>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QRegularExpression>
#include <QSet>
#include <QString>
#include <QStringList>

#include "core/settings/SettingsScope.h"

using namespace NereusSDR;

namespace {

// ---------------------------------------------------------------------
// Mechanical key extraction.
//
// Regex-over-source, not a real parser -- same tradeoff
// tst_core_has_no_gui_includes.cpp makes for the #include boundary, and
// for the same reason: a real C++ parse of this tree is not a
// proportionate cost for a guard test. Three limits fall out of that
// choice, all handled explicitly rather than silently mis-extracting:
//
//   1. Only a DIRECTLY quoted literal (bare "..." or wrapped in
//      QStringLiteral(...)/QLatin1String(...)) is recognised as a key
//      argument. A key built by concatenation at the call site (e.g.
//      AudioDeviceConfig.cpp's `s.value(base + QStringLiteral("Gain"))`)
//      is invisible to this scan. Every family built that way in this
//      tree was read by hand while writing this file's rule table and
//      the accompanying report; none of them changes either assertion's
//      outcome below (they are either exempted "audio/*" families or
//      already covered some other way), so this is a known, accepted gap
//      rather than a silent one. Six further families realize to
//      "<base>_%1"-style index/id-suffixed keys invisible for the same
//      reason (ContainerData_%1/ContainerItems_%1 in
//      ContainerManager.cpp, keyed on a QUuid container id;
//      SpotBandFilter_%1/spotListBandPill_%1 keyed on a band label like
//      "20m"; SpotSourceFilter_%1/spotListSourcePill_%1 keyed on a
//      source label, all in SpotHubDialog.cpp) -- all six live outside
//      even src/gui/setup/ (they're in src/gui/ directly), all six match
//      no rule in the table either stripped or unstripped, so all six
//      classify OperatorLocal regardless, correctly.
//   2. hardwareValue(mac, key, ...) / setHardwareValue(mac, key, value)
//      pass a MAC as the first argument and the bare per-radio key
//      literal as the second; AppSettings::hardwareValue/setHardwareValue
//      (AppSettings.cpp:974, :981) prepend "hardware/<mac>/" internally
//      before the key ever reaches the canonical setValue()/value()
//      funnel. Extracting the bare literal ("radioInfo/sampleRate")
//      would test a string classifySettingsKey() is never actually asked
//      to classify in production -- Task 15 only ever sees the fully
//      qualified form via AppSettings::setChangeHook(). So every
//      hardwareValue/setHardwareValue literal is reconstructed here with
//      a synthetic "hardware/<mac>/" prefix before being added to the key
//      set, mirroring what the real accessor does. The same
//      reconstruction is applied to any first-argument literal that
//      itself starts with the literal text "%1/" (AlexController.cpp/
//      ApolloController.cpp build several keys by hand via
//      QStringLiteral("%1/...").arg(base) where base is always
//      persistenceKey() == "hardware/<mac>/alex/antenna" or
//      "hardware/<mac>/apollo" -- AlexController.cpp:360-363,
//      ApolloController.h:141) -- see reconstructFirstArgKey() below.
//      Fix round 1 (review) moved this handling here from a
//      classifySettingsKey rule that existed ONLY to satisfy this scan's
//      own limitation: production code has no legitimate reason to
//      special-case the literal substring "%1", since QString::arg()
//      always resolves it before a real key is ever used, and a rule
//      that did would classify a genuinely malformed key (a real .arg()
//      call forgotten somewhere) Station -- the direction
//      SettingsScope.h's own default-OperatorLocal rationale calls the
//      less recoverable one.
//   3. The receiver scoping below (see "Receiver is scoped...") cannot
//      see an UNQUALIFIED call inside AppSettings.cpp's own member
//      functions -- a bare value(key)/setValue(key,v)/remove(key) with
//      no "s."/"settings."/"as."/"AppSettings::instance()." prefix at
//      all, which compiles fine as an implicit self-call from inside the
//      class's own methods. Fix round 1 (review) found 10 such call
//      sites / 7 distinct literals in AppSettings.cpp: the "radios/*"
//      pair (AppSettings.cpp:942 value, :948 remove, :950 setValue for
//      "radios/lastConnected"; :957 value, :964 setValue for
//      "radios/discoveryProfile") plus five one-shot v0.3.0 schema
//      migration remove() calls inside ensureSettingsAtVersion()
//      (AppSettings.cpp:1183-1186, :1199: "DisplayAverageMode",
//      "DisplayPeakHold", "DisplayPeakHoldDelayMs",
//      "DisplayReverseWaterfallScroll", "DisplayAverageAlpha" -- all
//      legacy keys being deleted, not settings anything still reads).
//      "radios/*" is now a hand-seeded prefix rule in
//      SettingsScope.cpp (OperatorLocal, confirmed correct by grepping
//      DaemonApp.cpp/DaemonConfig.cpp for zero hits -- the daemon never
//      touches this namespace) and pinned in knownExamples_data() below,
//      the same way "Slice" is hand-seeded for a different blind spot.
//      The five migration removes are legacy/dead keys with no live
//      classification decision to make and are not otherwise addressed.
//
// Receiver is scoped to the three identifiers this tree actually binds
// AppSettings::instance() to by reference ('s', 'settings', 'as' -- every
// other binding seen is a one-shot non-reference result variable, e.g.
// `QString addr = AppSettings::instance().value(...).toString()`, which
// the direct-chain branch below already covers) or the direct
// `AppSettings::instance()` chain. Without this scoping, `.value(`/
// `.contains(`/`.remove(` alone match constructs having nothing to do
// with AppSettings -- QJsonObject::value(), QMap<QString,QString>::value/
// contains/remove -- and a codebase-wide sweep for just those method
// names pulls in dozens of unrelated string literals (JSON field names
// like "Baud"/"Format"/"Host" out of ExternalVariableEngine.cpp, PGXL/
// TGXL frame keys like "fwd"/"swr"/"state" out of RadioModel.cpp and
// TunerModel.cpp). The `\b` word boundaries matter: without them, `s`
// alone matches the trailing letter of `kvs`/`settings2`/any other
// identifier ending in "s".
// Plain escaped literals rather than raw string literals (R"(...)"):
// moc's own pre-parser choked on an embedded '"' inside a raw string here
// ("missing ')' in macro usage"), even though every affected function is
// a free function nowhere near the QObject-derived class below -- moc
// scans the whole translation unit, not just the class. Matches
// tst_core_has_no_gui_includes.cpp's own regex literals, which use the
// same escaped style for the same reason.
QRegularExpression firstArgKeyRegex()
{
    static const QRegularExpression re(
        QStringLiteral(
            "(?:\\bs\\b|\\bsettings\\b|\\bas\\b|AppSettings::instance\\(\\))"
            "\\s*\\.\\s*(value|setValue|contains|remove|stationValue|setStationValue)\\s*\\(\\s*"
            "(?:QStringLiteral|QLatin1String)?\\s*\\(?\\s*\"((?:[^\"\\\\]|\\\\.)*)\"\\s*\\)?"));
    return re;
}

QRegularExpression secondArgKeyRegex()
{
    static const QRegularExpression re(
        QStringLiteral(
            "(?:\\bs\\b|\\bsettings\\b|\\bas\\b|AppSettings::instance\\(\\))"
            "\\s*\\.\\s*(hardwareValue|setHardwareValue)\\s*\\([^,()]*(?:\\([^()]*\\))?[^,()]*,\\s*"
            "(?:QStringLiteral|QLatin1String)?\\s*\\(?\\s*\"((?:[^\"\\\\]|\\\\.)*)\"\\s*\\)?"));
    return re;
}

// True if a "//" appears on the same source line before matchStart --
// good enough to reject AppSettings.h's own usage-example doc comment
// (`//   s.setValue("LastConnectedRadioMac", ...)`, a literal that reads
// as a real call site to a regex that cannot tell code from prose) without
// the false-negative risk of also stripping block comments: an earlier
// draft of this scan stripped /* ... */ spans with a second regex and it
// silently swallowed real code in at least one large file (a stray '/'
// '*' pairing well before the code, non-greedy match landing on the wrong
// '*/'), which is a worse failure mode than the one false-positive comment
// example this guards against.
bool isLineCommented(const QString& text, qsizetype matchStart)
{
    const qsizetype lineStart = text.lastIndexOf(QLatin1Char('\n'), matchStart) + 1;
    return text.indexOf(QStringLiteral("//"), lineStart) >= 0
        && text.indexOf(QStringLiteral("//"), lineStart) < matchStart;
}

// One synthetic MAC segment stands in for every hardwareValue/
// setHardwareValue call site (and every "%1/"-prefixed literal, see
// reconstructFirstArgKey() below): classifySettingsKey's "hardware/"
// prefix rule does not (and per the brief's own pennyExtCtrl example,
// must not) care what occupies that segment.
const QString kSyntheticMac = QStringLiteral("hardware/00:11:22:33:44:55/");

// A first-argument literal starting with "%1/" is AlexController.cpp/
// ApolloController.cpp's hand-built "hardware/<mac>/..." shape with the
// resolved prefix still a template placeholder (see "Three limits" item 2
// above). Reconstruct it exactly like a hardwareValue() literal: strip
// the 3-character "%1/" and prepend the same synthetic hardware/<mac>/
// segment. A key with no such prefix is returned unchanged.
QString reconstructFirstArgKey(const QString& key)
{
    static const QString kTemplatePrefix = QStringLiteral("%1/");
    if (key.startsWith(kTemplatePrefix)) {
        return kSyntheticMac + key.sliced(kTemplatePrefix.size());
    }
    return key;
}

struct ScanResult {
    QSet<QString> keys;
    int filesScanned = 0;
};

ScanResult scanTree(const QString& absoluteDir)
{
    ScanResult result;
    QDirIterator it(absoluteDir, {QStringLiteral("*.h"), QStringLiteral("*.cpp")},
                    QDir::Files, QDirIterator::Subdirectories);
    const QRegularExpression firstArg = firstArgKeyRegex();
    const QRegularExpression secondArg = secondArgKeyRegex();
    while (it.hasNext()) {
        const QString path = it.next();
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) { continue; }
        ++result.filesScanned;
        const QString text = QString::fromUtf8(f.readAll());

        auto matches = firstArg.globalMatch(text);
        while (matches.hasNext()) {
            const QRegularExpressionMatch m = matches.next();
            if (isLineCommented(text, m.capturedStart(0))) { continue; }
            result.keys.insert(reconstructFirstArgKey(m.captured(2)));
        }

        auto hwMatches = secondArg.globalMatch(text);
        while (hwMatches.hasNext()) {
            const QRegularExpressionMatch m = hwMatches.next();
            if (isLineCommented(text, m.capturedStart(0))) { continue; }
            result.keys.insert(kSyntheticMac + m.captured(2));
        }
    }
    return result;
}

// ---------------------------------------------------------------------
// Exemption list for assertion (a) -- core/models-touched keys that are
// correctly OperatorLocal despite that. Each entry is a deliberate
// finding, not a formality; see the report for the fuller writeup.
//
//   audio/*  (prefix, minus the two explicit-Station exceptions
//   classifySettingsKey itself carries: audio/DspRate, audio/
//   DspBlockSize) -- read and written from src/core: AudioEngine.cpp
//   ensureSpeakersOpen/ensureTxInputOpen (AudioDeviceConfig::
//   loadFromSettings("audio/Speakers"|"audio/TxInput")), setVacFeedbackParams
//   (audio/VacFeedback/<ch>/*), AppSettings.cpp's migrateVaxSchemaV1ToV2
//   (audio/Speakers/BitDepth seed, audio/FirstRunComplete,
//   audio/OutputDevice legacy removal). Also written by the DAEMON
//   itself: DaemonApp.cpp:220 sets audio/Speakers/DeviceName from
//   nereusd.conf's audio_device. None of that makes it Station: it is
//   local sound hardware selection on whichever machine is running the
//   code that opens the device, and that machine's own sound card list
//   is meaningless to any OTHER machine that key might sync to. The two
//   exceptions are real DSP engine parameters (WDSP buffer/sample rate),
//   not device selection, which is exactly why they are NOT in this
//   exemption list -- they classify Station on their own.
//
//   tx/preconnect/*  and  tx/OwnerSlot  -- same shape as audio/*, one
//   layer up the stack. TransmitModel.cpp:1436-1456/2403-2427 persists
//   the pre-connect TX mic-source fallback ("the user clicks the radio
//   button in Setup -> Audio -> TX Input before connecting to a radio");
//   TransmitModel.cpp:387/397 persists which VaxSlot currently owns TX
//   audio. Both are about which LOCAL audio input feeds the transmitter,
//   the capture-side mirror of audio/Speakers/DeviceName, so the same
//   reasoning applies.
//
//   DisplayGridStep, DisplayProfileApplied -- read by
//   src/models/PanadapterModel (grid line spacing; a one-shot "have I
//   applied the smooth-defaults display profile" migration flag). Both
//   are cosmetic/bookkeeping for one client's own rendering, unrelated to
//   the four FftPoolConfig knobs pinned Station in knownExamples() above.
//
//   LogCategory_*  -- src/core/LogCategories.cpp persists per-category
//   qCDebug/qCInfo/qCWarning verbosity for whichever PROCESS is running:
//   the daemon and the GUI each have their own log file (see
//   tst_daemon_settings_profile.cpp's header), and a category relevant to
//   one is frequently noise on the other, so this is correctly
//   per-process rather than per-station.
//
//   SettingsSchemaVersion -- read from src/core by StationServer.cpp and
//   StationClient.cpp (remote-daemon R2 Task 18) so each end can advertise
//   its own AppSettings schema version in the section 7.0 handshake and
//   the client can detect skew. It is correctly OperatorLocal, and it is
//   load-bearing that it stays that way: the whole comparison depends on
//   each side reading the value from ITS OWN store. Were it Station, the
//   snapshot would overwrite the client's copy with the daemon's and the
//   two would agree by construction, permanently -- a skew check that can
//   never fire. AppSettings::ensureSettingsAtVersion() (CoreInit.cpp) is
//   what writes it, per machine, after that machine's own migrations run.
//
//   Not on this list: "radios/*". It is core-touched (AppSettings.cpp's
//   own lastConnected()/setLastConnected()/discoveryProfile()/
//   setDiscoveryProfile()), correctly classifies OperatorLocal, but is
//   invisible to this scan for a completely different reason ("Three
//   limits" item 3 above: unqualified self-calls, not a concatenated or
//   .arg()-built key) -- so it never reaches this exemption check at all,
//   and does not belong on a list of keys the sweep found and had to be
//   told to ignore.
const QStringList kCoreExemptPrefixes = {
    QStringLiteral("audio/"),
    QStringLiteral("Audio/"),      // capital-A: LinuxBackendPreferred,
                                    // Vax%1/NodeDescription -- a DIFFERENT,
                                    // case-sensitive namespace from
                                    // lowercase "audio/", same local-
                                    // machine-audio reasoning.
    QStringLiteral("tx/preconnect/"),
    QStringLiteral("LogCategory_"),
};

const QSet<QString> kCoreExemptExact = {
    QStringLiteral("tx/OwnerSlot"),
    QStringLiteral("DisplayGridStep"),
    QStringLiteral("DisplayProfileApplied"),
    QStringLiteral("SettingsSchemaVersion"),
};

// R-R3-42: keys of a server that src/core implements but only this
// computer's window runs. The TCI server (src/core/TciServer.cpp,
// TciProtocol.cpp) lives in src/core because it drives RadioModel, yet
// only MainWindow constructs one and the Core runs none
// (tciServerIsBuiltOnlyByTheWindow() below pins that). Its settings are
// read by that core code AND written by two Setup pages, which is the
// exact shape assertion (b) exists to reject for a Station key; for this
// family it is the intended shape, because both ends are this computer.
// Exempt from (a) and (b) both, by prefix, and only this prefix.
//
// Parity Task 19 (R-IOS-25; remote design section 6.4): the WSJT-X and
// SpotCollector listeners are the same shape. RadioModel (src/models) owns
// their clients and reads their settings, yet each computer runs its own
// (SpotSourceHost: the Core never starts them), since they listen for
// programs on the computer they run on.
const QStringList kThisComputerServerPrefixes = {
    QStringLiteral("Tci"),
    QStringLiteral("Wsjtx"),
    QStringLiteral("SpotCollector"),
};

bool isThisComputerServerKey(const QString& key)
{
    for (const QString& prefix : kThisComputerServerPrefixes) {
        if (key.startsWith(prefix)) { return true; }
    }
    return false;
}

bool isCoreExempt(const QString& key)
{
    if (isThisComputerServerKey(key)) { return true; }
    if (kCoreExemptExact.contains(key)) { return true; }
    for (const QString& prefix : kCoreExemptPrefixes) {
        if (key.startsWith(prefix)) { return true; }
    }
    return false;
}

// ---------------------------------------------------------------------
// R-R3-21 / R-R3-23: the ThisComputer page sweep.
//
// A Setup page registered SetupScope::ThisComputer works the same in a
// remote window whether or not it is connected, and nothing on it is
// disabled while the Core's settings are unavailable. That is only honest
// if every setting it writes is this computer's own. The pages and their
// classes are read from SetupDialog.cpp's registrations, so a new
// ThisComputer page is covered the day it is added; each class's own
// member-function bodies (out of line in any src/gui file, and inline in
// its class declaration) are scanned with the same key regexes as the
// completeness sweep.

// Text from `open` (the index of an opening brace) through its matching
// closing brace, or empty when unbalanced. Braces inside string and
// character literals are skipped; comments are not special-cased, which
// is good enough for these files (a stray brace in a comment would show
// up as an unbalanced body, and the floors below would fail loudly).
QString bracedBody(const QString& text, qsizetype open)
{
    int depth = 0;
    QChar quote;
    for (qsizetype i = open; i < text.size(); ++i) {
        const QChar c = text.at(i);
        if (!quote.isNull()) {
            if (c == QLatin1Char('\\')) { ++i; continue; }
            if (c == quote) { quote = QChar(); }
            continue;
        }
        if (c == QLatin1Char('"') || c == QLatin1Char('\'')) { quote = c; continue; }
        if (c == QLatin1Char('{')) { ++depth; }
        if (c == QLatin1Char('}') && --depth == 0) {
            return text.mid(open, i - open + 1);
        }
    }
    return QString();
}

struct ThisComputerPage {
    QString label;
    QStringList classes;
};

// Every `registerPage(parent, "Label", SetupScope::ThisComputer, ...)` in
// SetupDialog.cpp, with the classes its factory constructs (`new Class(`
// up to the next registration).
QList<ThisComputerPage> thisComputerPages(const QString& setupDialogSource)
{
    static const QRegularExpression registration(QStringLiteral(
        "registerPage\\(\\s*[\\w>-]+\\s*,\\s*\"([^\"]+)\"\\s*,\\s*SetupScope::(\\w+)"));
    static const QRegularExpression constructed(QStringLiteral("\\bnew\\s+([A-Z]\\w+)\\s*[({;]"));
    QList<QRegularExpressionMatch> matches;
    auto it = registration.globalMatch(setupDialogSource);
    while (it.hasNext()) { matches << it.next(); }
    QList<ThisComputerPage> pages;
    for (int i = 0; i < matches.size(); ++i) {
        if (matches.at(i).captured(2) != QStringLiteral("ThisComputer")) { continue; }
        const qsizetype begin = matches.at(i).capturedEnd(0);
        const qsizetype end = (i + 1 < matches.size()) ? matches.at(i + 1).capturedStart(0)
                                                        : setupDialogSource.size();
        ThisComputerPage page{matches.at(i).captured(1), {}};
        auto classes = constructed.globalMatch(setupDialogSource.mid(begin, end - begin));
        while (classes.hasNext()) {
            const QString name = classes.next().captured(1);
            if (!name.startsWith(QLatin1Char('Q')) && !page.classes.contains(name)) {
                page.classes << name;
            }
        }
        pages << page;
    }
    return pages;
}

// Member-function bodies (out of line) and the class declaration body
// (inline members) of `className`, across every .h/.cpp under `root`.
QStringList classBodies(const QString& root, const QString& className)
{
    const QRegularExpression definition(
        QStringLiteral("\\b%1::~?\\w+\\s*\\(").arg(QRegularExpression::escape(className)));
    const QRegularExpression declaration(
        QStringLiteral("\\bclass\\s+%1\\b[^;{]*\\{").arg(QRegularExpression::escape(className)));
    QStringList bodies;
    QDirIterator it(root, {QStringLiteral("*.h"), QStringLiteral("*.cpp")},
                    QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QFile f(it.next());
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) { continue; }
        const QString text = QString::fromUtf8(f.readAll());
        if (!text.contains(className)) { continue; }
        auto defs = definition.globalMatch(text);
        while (defs.hasNext()) {
            const QRegularExpressionMatch m = defs.next();
            // A definition reaches its body's `{` before any `;`; a call
            // or a declaration reaches `;` first.
            const qsizetype brace = text.indexOf(QLatin1Char('{'), m.capturedEnd(0));
            const qsizetype semi = text.indexOf(QLatin1Char(';'), m.capturedEnd(0));
            if (brace < 0 || (semi >= 0 && semi < brace)) { continue; }
            const QString body = bracedBody(text, brace);
            if (!body.isEmpty()) { bodies << body; }
        }
        auto decls = declaration.globalMatch(text);
        while (decls.hasNext()) {
            const QRegularExpressionMatch m = decls.next();
            const QString body = bracedBody(text, m.capturedEnd(0) - 1);
            if (!body.isEmpty()) { bodies << body; }
        }
    }
    return bodies;
}

QSet<QString> keysIn(const QString& text)
{
    QSet<QString> keys;
    auto first = firstArgKeyRegex().globalMatch(text);
    while (first.hasNext()) {
        const QRegularExpressionMatch m = first.next();
        if (isLineCommented(text, m.capturedStart(0))) { continue; }
        keys.insert(reconstructFirstArgKey(m.captured(2)));
    }
    auto second = secondArgKeyRegex().globalMatch(text);
    while (second.hasNext()) {
        const QRegularExpressionMatch m = second.next();
        if (isLineCommented(text, m.capturedStart(0))) { continue; }
        keys.insert(kSyntheticMac + m.captured(2));
    }
    return keys;
}

} // namespace

class TstSettingsScope : public QObject {
    Q_OBJECT
private slots:

    // ---- Step 1: table-driven known examples --------------------------
    void knownExamples_data()
    {
        QTest::addColumn<QString>("key");
        QTest::addColumn<int>("expected"); // SettingsScope, as int

        // Brief-mandated worked examples.
        QTest::newRow("DisplayFftSize is Station (FftPoolConfig)")
            << QStringLiteral("DisplayFftSize") << int(SettingsScope::Station);
        QTest::newRow("DisplayNoiseFloorColor is OperatorLocal (rendering)")
            << QStringLiteral("DisplayNoiseFloorColor") << int(SettingsScope::OperatorLocal);
        // R-R3-42: the TCI server runs on this computer, in a remote window
        // as in a local one, so its settings are this computer's.
        QTest::newRow("TciServerPort is OperatorLocal (this computer's TCI server)")
            << QStringLiteral("TciServerPort") << int(SettingsScope::OperatorLocal);
        QTest::newRow("TciEmulateSunSDR2Pro is OperatorLocal (this computer's TCI server)")
            << QStringLiteral("TciEmulateSunSDR2Pro") << int(SettingsScope::OperatorLocal);
        QTest::newRow("TciSliceAGain is OperatorLocal (the TCI applet's gain slider)")
            << QStringLiteral("TciSliceAGain") << int(SettingsScope::OperatorLocal);
        // R-R3-44: a remote window's VAX channel per Core and slice is this
        // computer's (RemoteVaxRouter::settingsKey); the Core's own
        // Slice<N>/VaxChannel stays the Core's.
        QTest::newRow("RemoteVax/<core>/Slice1/Channel is OperatorLocal (this computer's VAX)")
            << QStringLiteral("RemoteVax/3f1a9c0d22b4e6f7/Slice1/Channel")
            << int(SettingsScope::OperatorLocal);
        QTest::newRow("Slice1/VaxChannel is Station (the Core's own VAX)")
            << QStringLiteral("Slice1/VaxChannel") << int(SettingsScope::Station);
        QTest::newRow("audio/Vax1/RxGain is OperatorLocal (this computer's VAX gain)")
            << QStringLiteral("audio/Vax1/RxGain") << int(SettingsScope::OperatorLocal);
        QTest::newRow("audio/Vax1/Muted is OperatorLocal (this computer's VAX mute)")
            << QStringLiteral("audio/Vax1/Muted") << int(SettingsScope::OperatorLocal);
        QTest::newRow("ContainerWorkspace remains client-local")
            << QStringLiteral("ContainerWorkspace") << int(SettingsScope::OperatorLocal);
        QTest::newRow("ContainerWorkspaceBackup remains client-local")
            << QStringLiteral("ContainerWorkspaceBackup") << int(SettingsScope::OperatorLocal);
        QTest::newRow("TciLogWindowGeometry is OperatorLocal (GUI dialog geometry)")
            << QStringLiteral("TciLogWindowGeometry") << int(SettingsScope::OperatorLocal);
        QTest::newRow("hardware/oc/pennyExtCtrl is Station (oc is a literal segment, not a MAC)")
            << QStringLiteral("hardware/oc/pennyExtCtrl") << int(SettingsScope::Station);

        // The other three FftPoolConfig knobs (MainWindow.cpp:1512's "the
        // four display AppSettings-sourced knobs"). MainWindow.cpp is
        // outside all three trees the completeness sweep scans, so these
        // three are NOT covered by that sweep -- only by these rows.
        QTest::newRow("DisplayFftWindow is Station (FftPoolConfig)")
            << QStringLiteral("DisplayFftWindow") << int(SettingsScope::Station);
        QTest::newRow("DisplayHzPerBinTarget is Station (FftPoolConfig)")
            << QStringLiteral("DisplayHzPerBinTarget") << int(SettingsScope::Station);
        QTest::newRow("DisplaySpectrumFps is Station (the straddle -- see SettingsScope.cpp)")
            << QStringLiteral("DisplaySpectrumFps") << int(SettingsScope::Station);
        // Parity ruling C12: the per-band grid dB max and min are the Core's.
        QTest::newRow("DisplayGridMax_20m is Station (the Core's per-band grid)")
            << QStringLiteral("DisplayGridMax_20m") << int(SettingsScope::Station);
        QTest::newRow("DisplayGridMin_160m is Station (the Core's per-band grid)")
            << QStringLiteral("DisplayGridMin_160m") << int(SettingsScope::Station);

        // The "audio/" split: DspRate/DspBlockSize are real WDSP engine
        // parameters (Station); Speakers/DeviceName is local sound
        // hardware selection (OperatorLocal) even though both are
        // core-touched under the same "audio/" prefix.
        QTest::newRow("audio/DspRate is Station (WDSP engine parameter)")
            << QStringLiteral("audio/DspRate") << int(SettingsScope::Station);
        QTest::newRow("audio/DspBlockSize is Station (WDSP engine parameter)")
            << QStringLiteral("audio/DspBlockSize") << int(SettingsScope::Station);
        QTest::newRow("audio/Speakers/DeviceName is OperatorLocal (local sound device)")
            << QStringLiteral("audio/Speakers/DeviceName") << int(SettingsScope::OperatorLocal);
        QTest::newRow("tx/preconnect/Mic_Source is OperatorLocal (local mic device)")
            << QStringLiteral("tx/preconnect/Mic_Source") << int(SettingsScope::OperatorLocal);

        // R-R3-23 / R-R3-36: Setup > Audio > Devices (a ThisComputer page)
        // and the PC Mic half of TX Input save these in a remote window.
        // Every field AudioDeviceConfig::saveToSettings writes under the
        // three card prefixes, plus the Headphones enable, must stay on
        // this computer: were one Station, picking a sound card in a
        // remote window would write it to the Core. Built by concatenation
        // (AudioDeviceConfig.cpp), so the completeness sweep cannot see
        // them; these rows are their only cover.
        for (const char* card : {"Speakers", "Headphones", "TxInput"}) {
            for (const char* field : {"DriverApi", "DeviceName", "SampleRate", "BitDepth",
                                      "Channels", "BufferSamples", "ExclusiveMode",
                                      "EventDriven", "BypassMixer", "ManualLatencyMs"}) {
                const QString key = QStringLiteral("audio/%1/%2")
                                        .arg(QLatin1String(card), QLatin1String(field));
                QTest::newRow(qPrintable(key + QStringLiteral(" is OperatorLocal (this computer's device)")))
                    << key << int(SettingsScope::OperatorLocal);
            }
        }
        QTest::newRow("audio/Headphones/Enabled is OperatorLocal (this computer's device)")
            << QStringLiteral("audio/Headphones/Enabled") << int(SettingsScope::OperatorLocal);
        // R-R3-45: where MON plays is this computer's choice too.
        QTest::newRow("audio/TxMonitor/Output is OperatorLocal (this computer's output)")
            << QStringLiteral("audio/TxMonitor/Output") << int(SettingsScope::OperatorLocal);
        // The radio's own microphone input stays with the radio: TX Input's
        // mic source selector is a Core control in a remote window.
        QTest::newRow("hardware/<mac>/tx/Mic_Source is Station (the radio's mic input)")
            << QStringLiteral("hardware/00:1C:2D:05:37:2A/tx/Mic_Source")
            << int(SettingsScope::Station);
        // R-R3-10: what a remote window's Advanced Reset removes (this
        // computer's audio/* keys) and what it leaves for the Core.
        for (const char* key : {"audio/VacFeedback/1/Gain", "audio/SendIqToVax",
                                "audio/TxMonitorToVax", "audio/MuteVaxDuringTxOnOtherSlice",
                                "audio/FirstRunComplete", "audio/LastDetectedCables",
                                "audio/Vax1/DeviceName"}) {
            QTest::newRow(qPrintable(QStringLiteral("%1 is OperatorLocal (Advanced Reset removes it)")
                                         .arg(QLatin1String(key))))
                << QString::fromLatin1(key) << int(SettingsScope::OperatorLocal);
        }

        // A realistic fully-qualified hardwareValue-routed key (a real
        // MAC-shaped segment this time, not "oc") -- proves the
        // "hardware/" prefix rule Task 15 actually relies on, since every
        // hardwareValue()/setHardwareValue() key it will ever see already
        // carries this shape (AppSettings.cpp:974, :981).
        QTest::newRow("hardware/<mac>/radioInfo/sampleRate is Station")
            << QStringLiteral("hardware/00:1C:2D:05:37:2A/radioInfo/sampleRate")
            << int(SettingsScope::Station);

        QTest::newRow("Region is Station (band-plan/TX-legality is where the radio is)")
            << QStringLiteral("Region") << int(SettingsScope::Station);
        QTest::newRow("StationCallsign is Station")
            << QStringLiteral("StationCallsign") << int(SettingsScope::Station);

        // radios/lastConnected: invisible to the completeness sweep
        // (AppSettings.cpp's own lastConnected()/setLastConnected() call
        // value()/setValue()/remove() with no receiver prefix at all --
        // "Three limits" item 3 in this file's header comment), so nothing
        // else in this suite forces an answer for it. OperatorLocal is
        // correct (confirmed: DaemonApp.cpp/DaemonConfig.cpp never touch
        // "radios/" at all -- the daemon picks its radio from
        // nereusd.conf), and this row is what actually pins it.
        QTest::newRow("radios/lastConnected is OperatorLocal (daemon never touches this namespace)")
            << QStringLiteral("radios/lastConnected") << int(SettingsScope::OperatorLocal);

        // ---- Whole-branch review, Important 3 ----------------------------
        //
        // The "FreeDv" prefix rule's justification is that RadioModel owns
        // the client instance and the dialogs "only present what RadioModel
        // already collected". True of the four identity/connection keys
        // pinned first below, and false of the ten after them: those are
        // the FreeDV Reporter window's own table geometry, sort state,
        // column visibility, per-column filters and display-unit toggles,
        // and their ONLY reader or writer anywhere in the tree is
        // src/gui/FreeDVReporterDialog.cpp or src/gui/SpotHubDialog.cpp.
        //
        // Not covered by either completeness assertion below, in either
        // direction: those scan src/core, src/models and src/gui/setup, and
        // these ten live in src/gui/ directly. Both assertions test for
        // UNDER-classification only, which is why this over-classification
        // survived to a whole-branch review. These rows are the pin.
        QTest::newRow("FreeDvReporter/Callsign is Station (RadioModel seeds the client)")
            << QStringLiteral("FreeDvReporter/Callsign") << int(SettingsScope::Station);
        QTest::newRow("FreeDvReporter/GridSquare is Station (the STATION's grid)")
            << QStringLiteral("FreeDvReporter/GridSquare") << int(SettingsScope::Station);
        QTest::newRow("FreeDvReporter/Message is Station (RadioModel.cpp reads it)")
            << QStringLiteral("FreeDvReporter/Message") << int(SettingsScope::Station);
        QTest::newRow("FreeDvReporter/ServerUrl is Station (which server the client dials)")
            << QStringLiteral("FreeDvReporter/ServerUrl") << int(SettingsScope::Station);

        QTest::newRow("FreeDvReporter/ColumnWidths is OperatorLocal (this window's table)")
            << QStringLiteral("FreeDvReporter/ColumnWidths") << int(SettingsScope::OperatorLocal);
        QTest::newRow("FreeDvReporter/SortColumn is OperatorLocal (this window's table)")
            << QStringLiteral("FreeDvReporter/SortColumn") << int(SettingsScope::OperatorLocal);
        QTest::newRow("FreeDvReporter/SortAscending is OperatorLocal (this window's table)")
            << QStringLiteral("FreeDvReporter/SortAscending") << int(SettingsScope::OperatorLocal);
        QTest::newRow("FreeDvReporter/VisibleColumns is OperatorLocal (this window's table)")
            << QStringLiteral("FreeDvReporter/VisibleColumns") << int(SettingsScope::OperatorLocal);
        QTest::newRow("FreeDvReporter/ColumnFilters is OperatorLocal (this window's table)")
            << QStringLiteral("FreeDvReporter/ColumnFilters") << int(SettingsScope::OperatorLocal);
        QTest::newRow("FreeDvReporter/BandFilter is OperatorLocal (this window's view filter)")
            << QStringLiteral("FreeDvReporter/BandFilter") << int(SettingsScope::OperatorLocal);
        // iPhone plan Task 22 / parity Task 20: the Core runs FreeDV
        // Reporter, so "Hide my station" and the saved status messages are
        // the Core's.
        QTest::newRow("FreeDvReporter/Hidden is Station (the Core's Hide my station)")
            << QStringLiteral("FreeDvReporter/Hidden") << int(SettingsScope::Station);
        QTest::newRow("FreeDvReporter/SavedMessages is Station (the Core's saved messages)")
            << QStringLiteral("FreeDvReporter/SavedMessages") << int(SettingsScope::Station);
        QTest::newRow("FreeDvReporter/DistanceMiles is OperatorLocal (a display unit)")
            << QStringLiteral("FreeDvReporter/DistanceMiles") << int(SettingsScope::OperatorLocal);
        QTest::newRow("FreeDvReporter/DirectionAsCardinal is OperatorLocal (a display unit)")
            << QStringLiteral("FreeDvReporter/DirectionAsCardinal")
            << int(SettingsScope::OperatorLocal);
        QTest::newRow("FreeDvReporter/FrequencyAsKhz is OperatorLocal (a display unit)")
            << QStringLiteral("FreeDvReporter/FrequencyAsKhz") << int(SettingsScope::OperatorLocal);
    }

    void knownExamples()
    {
        QFETCH(QString, key);
        QFETCH(int, expected);
        QCOMPARE(int(classifySettingsKey(key)), expected);
    }

    // ---- Step 3: per-pan suffix stripping ------------------------------
    void perPanSuffixIsStrippedBeforeMatching_data()
    {
        QTest::addColumn<QString>("suffixed");
        QTest::addColumn<QString>("base");

        QTest::newRow("Station key, pan 1")
            << QStringLiteral("DisplayFftSize_1") << QStringLiteral("DisplayFftSize");
        QTest::newRow("Station key, pan 12 (multi-digit)")
            << QStringLiteral("DisplayFftSize_12") << QStringLiteral("DisplayFftSize");
        QTest::newRow("OperatorLocal key, pan 3")
            << QStringLiteral("DisplayNoiseFloorColor_3") << QStringLiteral("DisplayNoiseFloorColor");
    }

    void perPanSuffixIsStrippedBeforeMatching()
    {
        QFETCH(QString, suffixed);
        QFETCH(QString, base);
        QCOMPARE(int(classifySettingsKey(suffixed)), int(classifySettingsKey(base)));
    }

    // Negative case: a real key whose only underscore is followed by
    // letters, not digits, must NOT be treated as pan-suffixed. Checked
    // against the literal string classifySettingsKey would need to
    // mis-strip to "TciSliceA" for this to accidentally pass for the
    // wrong reason (both "TciSliceA" and "TciSliceA_OutputSampleRate"
    // classify the same via the "Tci" prefix -- OperatorLocal since
    // R-R3-42 -- so a scope-only assertion here would not actually prove
    // the suffix survived).
    void nonDigitSuffixIsNotStripped()
    {
        const QString real = QStringLiteral("TciSliceA_OutputSampleRate");
        QCOMPARE(int(classifySettingsKey(real)), int(SettingsScope::OperatorLocal));
        // If stripping fired here it would classify "TciSliceA" instead,
        // which still resolves the same via the same prefix -- so also
        // pin a key that would flip OperatorLocal->something-else-entirely
        // under a wrong all-suffix-strip, to make a regression loud
        // instead of silent: RfKit_Ant1_Label's only "trailing" digit
        // sits before "_Label", not at the end, so an implementation that
        // stripped from the FIRST underscore instead of the last would
        // mangle it to "RfKit" and lose the antenna-label family. RfKit_
        // is a Station prefix either way, so this specifically exercises
        // "strip only a true trailing _<digits> run", not the resulting
        // scope.
        const QString label = QStringLiteral("RfKit_Ant1_Label");
        QCOMPARE(int(classifySettingsKey(label)), int(SettingsScope::Station));
    }

    // Fix wave minor 4 (R-R3-21 / R-R3-09): the Core owns exactly the notch
    // keys NotchModel::saveToSettings writes for its list and flags, not
    // every key that starts with "notch". The window's display preference
    // stays its own.
    void modelOwnedNotchKeysAreTheExactForms_data()
    {
        QTest::addColumn<QString>("key");
        QTest::addColumn<bool>("owned");
        QTest::newRow("count") << QStringLiteral("NotchCount") << true;
        QTest::newRow("global") << QStringLiteral("NotchGlobalEnabled") << true;
        QTest::newRow("auto") << QStringLiteral("NotchAutoIncrease") << true;
        QTest::newRow("centre 0") << QStringLiteral("Notch0Center") << true;
        QTest::newRow("width 12") << QStringLiteral("Notch12Width") << true;
        QTest::newRow("active 1023") << QStringLiteral("Notch1023Active") << true;
        QTest::newRow("any case") << QStringLiteral("notchcount") << true;
        QTest::newRow("visual") << QStringLiteral("NotchVisualEnabled") << false;
        QTest::newRow("leading zero") << QStringLiteral("Notch01Center") << false;
        QTest::newRow("no index") << QStringLiteral("NotchCenter") << false;
        QTest::newRow("other field") << QStringLiteral("Notch0Depth") << false;
        QTest::newRow("prefix only") << QStringLiteral("Notch") << false;
        QTest::newRow("other notch key") << QStringLiteral("NotchFutureSetting") << false;
        QTest::newRow("suffix") << QStringLiteral("NotchCountOld") << false;
        QTest::newRow("sign") << QStringLiteral("Notch-1Center") << false;
    }
    void modelOwnedNotchKeysAreTheExactForms()
    {
        QFETCH(QString, key);
        QFETCH(bool, owned);
        QCOMPARE(isModelOwnedNotchSettingsKey(key), owned);
        QCOMPARE(isModelOwnedDspSettingsKey(key), owned);
    }

    // ---- R-R3-21 / R-R3-23: ThisComputer pages write only this
    // computer's settings. A disconnected remote window leaves these
    // pages fully usable, so a Core key here would be changed while the
    // Core cannot hear about it. --------------------------------------
    void everyThisComputerPageKeyIsThisComputers()
    {
        const QString root = QStringLiteral(NEREUS_SOURCE_DIR);
        QFile dialog(root + QStringLiteral("/src/gui/SetupDialog.cpp"));
        QVERIFY2(dialog.open(QIODevice::ReadOnly | QIODevice::Text),
                 "SetupDialog.cpp not found; NEREUS_SOURCE_DIR is probably wrong");
        const QList<ThisComputerPage> pages =
            thisComputerPages(QString::fromUtf8(dialog.readAll()));

        // Floors: Task 1 registered 19 ThisComputer leaves and the R3 Setup
        // fix wave five more (Filter Presets, Spectrum Peaks, Waterfall
        // Defaults, 3D View, Export / Import); a registration regex that
        // stopped matching would otherwise pass vacuously. Startup &
        // Preferences became Mixed when its callsign and grid were
        // connected to the station identity (R3 controls that work), and
        // Filter Presets became a Core page when the presets moved to the
        // Core (iPhone app Task 19, D40).
        QVERIFY2(pages.size() >= 20,
                 qPrintable(QStringLiteral("only %1 ThisComputer registrations found")
                                .arg(pages.size())));

        QStringList offenders;
        int bodiesScanned = 0;
        QSet<QString> keysSeen;
        for (const ThisComputerPage& page : pages) {
            QVERIFY2(!page.classes.isEmpty(),
                     qPrintable(QStringLiteral("no page class found for ThisComputer leaf \"%1\"")
                                    .arg(page.label)));
            for (const QString& className : page.classes) {
                const QStringList bodies = classBodies(root + QStringLiteral("/src/gui"), className);
                QVERIFY2(!bodies.isEmpty(),
                         qPrintable(QStringLiteral("no source found for %1 (leaf \"%2\")")
                                        .arg(className, page.label)));
                bodiesScanned += bodies.size();
                for (const QString& body : bodies) {
                    for (const QString& key : keysIn(body)) {
                        keysSeen.insert(key);
                        if (classifySettingsKey(key) != SettingsScope::OperatorLocal) {
                            offenders << QStringLiteral("%1 (%2, leaf \"%3\")")
                                             .arg(key, className, page.label);
                        }
                    }
                }
            }
        }
        QVERIFY2(bodiesScanned > 40,
                 qPrintable(QStringLiteral("only %1 function bodies scanned").arg(bodiesScanned)));
        QStringList seen(keysSeen.cbegin(), keysSeen.cend());
        seen.sort();
        // Most ThisComputer leaves are placeholders or build their keys at
        // run time (the Devices cards, "audio/<card>/..."), so the literal
        // population is small: 4 when this was written. The canary is a key
        // Appearance > Meter Styles writes directly; losing it means the
        // class-body extraction broke, not that the page changed.
        QVERIFY2(keysSeen.contains(QStringLiteral("AppearanceSmallModeFilterOnVfos"))
                     && keysSeen.size() >= 3,
                 qPrintable(QStringLiteral("only %1 keys found on ThisComputer pages; the "
                                           "extraction probably broke: %2")
                                .arg(keysSeen.size()).arg(seen.join(QStringLiteral(", ")))));
        offenders.sort();
        offenders.removeDuplicates();
        QVERIFY2(offenders.isEmpty(),
                 qPrintable(QStringLiteral("ThisComputer Setup pages write the Core's settings: %1")
                                .arg(offenders.join(QStringLiteral(", ")))));
    }

    // R-R3-44: Audio > VAX is a ThisComputer page, so the sweep above
    // covers every key its class writes.
    void vaxPageIsSweptAsThisComputers()
    {
        const QString root = QStringLiteral(NEREUS_SOURCE_DIR);
        QFile dialog(root + QStringLiteral("/src/gui/SetupDialog.cpp"));
        QVERIFY(dialog.open(QIODevice::ReadOnly | QIODevice::Text));
        const QList<ThisComputerPage> pages =
            thisComputerPages(QString::fromUtf8(dialog.readAll()));
        bool found = false;
        for (const ThisComputerPage& page : pages) {
            if (page.label == QStringLiteral("VAX")) {
                found = true;
                QVERIFY(page.classes.contains(QStringLiteral("AudioVaxPage")));
            }
        }
        QVERIFY2(found, "Audio > VAX is not registered ThisComputer");
    }

    // iPhone app Task 19 (D40): DSP > Filter Presets is a Core page, and
    // every preset key FilterPresetStore writes (filters/<mode>/<slot>/
    // {name,low,high}, built at run time, so the sweeps cannot see them) is
    // the Core's, for each of the 14 modes by SliceModel::modeName. Any key
    // the page writes literally is the Core's too.
    void filterPresetsAreTheCores()
    {
        const QString root = QStringLiteral(NEREUS_SOURCE_DIR);
        QFile dialog(root + QStringLiteral("/src/gui/SetupDialog.cpp"));
        QVERIFY(dialog.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString source = QString::fromUtf8(dialog.readAll());
        const QRegularExpression registration(QStringLiteral(
            "registerPage\\(\\s*[\\w>-]+\\s*,\\s*\"Filter Presets\"\\s*,\\s*SetupScope::(\\w+)"));
        const QRegularExpressionMatch match = registration.match(source);
        QVERIFY2(match.hasMatch(), "DSP > Filter Presets is not registered");
        QCOMPARE(match.captured(1), QStringLiteral("Core"));

        for (const char* mode : {"LSB", "USB", "DSB", "CWL", "CWU", "FM", "AM", "DIGU",
                                 "SPEC", "DIGL", "SAM", "DRM", "RADE-U", "RADE-L"}) {
            for (const char* field : {"name", "low", "high"}) {
                const QString key = QStringLiteral("filters/%1/9/%2")
                                        .arg(QLatin1String(mode), QLatin1String(field));
                QVERIFY2(classifySettingsKey(key) == SettingsScope::Station, qPrintable(key));
            }
        }

        const QStringList bodies =
            classBodies(root + QStringLiteral("/src/gui"), QStringLiteral("FilterPresetsSetupPage"));
        QVERIFY2(!bodies.isEmpty(), "no source found for FilterPresetsSetupPage");
        for (const QString& body : bodies) {
            for (const QString& key : keysIn(body)) {
                QVERIFY2(classifySettingsKey(key) == SettingsScope::Station, qPrintable(key));
            }
        }
    }

    // The sweep above must be able to fail: a Core key in a ThisComputer
    // class body is reported, an ordinary member call is not a definition.
    void thisComputerSweepSeesKeysInClassBodies()
    {
        const QString source = QStringLiteral(
            "    registerPage(general, \"Probe\", SetupScope::ThisComputer,\n"
            "                 [this] { return new ProbePage(m_model); });\n"
            "    registerPage(general, \"Other\", SetupScope::Core,\n"
            "                 [this] { return new OtherPage(m_model); });\n");
        const QList<ThisComputerPage> pages = thisComputerPages(source);
        QCOMPARE(pages.size(), 1);
        QCOMPARE(pages.first().label, QStringLiteral("Probe"));
        QCOMPARE(pages.first().classes, QStringList{QStringLiteral("ProbePage")});

        const QString body = QStringLiteral(
            "void ProbePage::apply()\n{\n"
            "    AppSettings::instance().setValue(QStringLiteral(\"Region\"), text);\n"
            "    auto& s = AppSettings::instance();\n"
            "    s.setValue(QStringLiteral(\"DisplayGridColor\"), \"}\");\n}\n");
        const qsizetype brace = body.indexOf(QLatin1Char('{'));
        const QSet<QString> keys = keysIn(bracedBody(body, brace));
        QCOMPARE(keys, (QSet<QString>{QStringLiteral("Region"),
                                      QStringLiteral("DisplayGridColor")}));
        QCOMPARE(classifySettingsKey(QStringLiteral("Region")), SettingsScope::Station);
    }

    // R-R3-42: the exemption above is honest only while the TCI server is
    // built by the window alone. A Core (nereusd, DaemonApp) that built one
    // would read settings this computer keeps to itself.
    //
    // R-R3-48: the one exception is the Core's station TCI server
    // (StationTciController). It reads the Tci keys of the Core's own
    // settings file, which no window writes (they are OperatorLocal), so it
    // runs on their defaults; its switch and port are its own StationTci_
    // keys. Any other builder still fails here.
    void tciServerIsBuiltOnlyByTheWindow()
    {
        const QString root = QStringLiteral(NEREUS_SOURCE_DIR);
        const QRegularExpression construct(
            QStringLiteral("new\\s+TciServer\\s*\\(|make_(unique|shared)<\\s*TciServer\\s*>"));
        QStringList builders;
        int scanned = 0;
        QDirIterator it(root + QStringLiteral("/src"), {QStringLiteral("*.cpp"), QStringLiteral("*.h")},
                        QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString path = it.next();
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) { continue; }
            ++scanned;
            if (construct.match(QString::fromUtf8(file.readAll())).hasMatch()) {
                builders << QDir(root).relativeFilePath(path);
            }
        }
        QVERIFY2(scanned > 300, qPrintable(QStringLiteral("only %1 files scanned").arg(scanned)));
        builders.sort();
        QCOMPARE(builders, (QStringList{QStringLiteral("src/core/StationTciController.cpp"),
                                        QStringLiteral("src/gui/MainWindow.cpp")}));
    }

    // ---- Step 4: the completeness sweep --------------------------------
    void completenessSweep()
    {
        const QString root = QStringLiteral(NEREUS_SOURCE_DIR);
        const ScanResult core = scanTree(root + QStringLiteral("/src/core"));
        const ScanResult models = scanTree(root + QStringLiteral("/src/models"));
        const ScanResult setup = scanTree(root + QStringLiteral("/src/gui/setup"));

        // Minimum-files-scanned guard, copied in shape from
        // tst_core_has_no_gui_includes.cpp:288-292 (its own comment: "A
        // wrong NEREUS_SOURCE_DIR ... scans zero files ... and would
        // otherwise report a false 'zero offenders' pass. Fail loudly
        // instead of passing vacuously."). This test has the identical
        // failure mode, plus a second one that guard did not: the
        // gui/setup floor must clear the 64-file depth-1 total (32 .cpp
        // + 32 .h -- scanTree() globs both), not the 32-file .cpp-only
        // count the brief and this file's own header comment quote for a
        // different purpose. Fix round 1 (review) found the original
        // floor here (40) sitting BETWEEN those two numbers: a
        // non-recursive scan of gui/setup returns exactly 64 files,
        // which passed a ">40" guard while silently losing 23 keys
        // (every hardware/.../alex/{hpf,lpf,bpf1}/* key and the whole OC
        // family among them) with the overlap set unaffected (the lost
        // keys are all setup-only), so assertion (b) stayed green with
        // no signal at all -- the exact regression this guard exists to
        // catch, undetected. 80 is 15% below the true recursive count
        // (94) and 25% above the 64-file non-recursive trap.
        const int coreModelsScanned = core.filesScanned + models.filesScanned;
        QVERIFY2(coreModelsScanned > 300,
                 qPrintable(QStringLiteral(
                     "only %1 core+models files scanned, NEREUS_SOURCE_DIR is probably wrong")
                                .arg(coreModelsScanned)));
        // Narrower floor on src/models alone: src/core's own 316 files
        // already clear 300 by itself, so a total failure of the
        // src/models scan (0 files) would still pass the combined floor
        // above. src/models measures 35 files; 20 is comfortably below
        // that without being fragile to normal file churn.
        QVERIFY2(models.filesScanned > 20,
                 qPrintable(QStringLiteral(
                     "only %1 src/models files scanned (out of ~35) -- "
                     "the src/models half of this scan is probably broken")
                                .arg(models.filesScanned)));
        QVERIFY2(setup.filesScanned > 80,
                 qPrintable(QStringLiteral(
                     "only %1 src/gui/setup files scanned (64 is the non-recursive "
                     "depth-1 total including headers -- this scan has stopped recursing)")
                                .arg(setup.filesScanned)));

        QSet<QString> coreModelsKeys = core.keys;
        coreModelsKeys.unite(models.keys);
        QVERIFY2(coreModelsKeys.size() > 50,
                 qPrintable(QStringLiteral("only %1 keys extracted from core+models, "
                                            "the extraction regex probably broke")
                                .arg(coreModelsKeys.size())));
        QVERIFY2(setup.keys.size() > 50,
                 qPrintable(QStringLiteral("only %1 keys extracted from src/gui/setup, "
                                            "the extraction regex probably broke")
                                .arg(setup.keys.size())));

        // ---- (a) the easy direction: core/models keys classify Station,
        // modulo the justified exemption list above. ----
        QStringList notStation;
        for (const QString& key : std::as_const(coreModelsKeys)) {
            if (isCoreExempt(key)) { continue; }
            if (classifySettingsKey(key) != SettingsScope::Station) {
                notStation << key;
            }
        }
        notStation.sort();
        QVERIFY2(notStation.isEmpty(),
                 qPrintable(QStringLiteral(
                     "%1 core/models key(s) classify OperatorLocal and are not in the "
                     "exemption list: %2")
                                .arg(notStation.size())
                                .arg(notStation.join(QStringLiteral(", ")))));

        // ---- (b) the valuable half: a key touched by BOTH a Setup page
        // and a core/models consumer must classify Station. No exemption
        // list here on purpose -- a key a Setup page writes into the same
        // namespace a core consumer reads is exactly the shape of the
        // failure this test exists to catch. The one carve-out is this
        // computer's own TCI server (kThisComputerServerPrefixes, R-R3-42),
        // whose keys must instead classify OperatorLocal. See this file's header
        // comment ("Fix round 1 (review) correction") for why this
        // assertion's LIVE reach, given (a) above already ran to
        // completion over the same coreModelsKeys, is narrower than "any
        // overlapping key": only the exempt subset of the overlap can
        // reach here with something to say that (a) did not already say
        // first. ----
        QSet<QString> overlap = coreModelsKeys;
        overlap.intersect(setup.keys);
        QVERIFY2(overlap.size() > 5,
                 qPrintable(QStringLiteral("only %1 overlap key(s) found between "
                                            "core/models and src/gui/setup -- suspiciously "
                                            "low, the extraction regex probably broke")
                                .arg(overlap.size())));

        QStringList overlapNotStation;
        for (const QString& key : std::as_const(overlap)) {
            if (isThisComputerServerKey(key)) {
                // This computer's own TCI server; see
                // kThisComputerServerPrefixes. It must then be this
                // computer's setting, never the Core's.
                QVERIFY2(classifySettingsKey(key) == SettingsScope::OperatorLocal,
                         qPrintable(QStringLiteral("%1 configures this computer's TCI "
                                                   "server but is not this computer's")
                                        .arg(key)));
                continue;
            }
            if (classifySettingsKey(key) != SettingsScope::Station) {
                overlapNotStation << key;
            }
        }
        overlapNotStation.sort();
        QVERIFY2(overlapNotStation.isEmpty(),
                 qPrintable(QStringLiteral(
                     "%1 key(s) appear in BOTH a Setup page and a core/models consumer "
                     "but do not classify Station: %2")
                                .arg(overlapNotStation.size())
                                .arg(overlapNotStation.join(QStringLiteral(", ")))));
    }
};

QTEST_MAIN(TstSettingsScope)
#include "tst_settings_scope.moc"
