// =================================================================
// src/server_main.cpp  (NereusSDR)
// =================================================================
// nereusd: headless NereusSDR daemon.
// Design: docs/architecture/2026-07-28-remote-daemon-architecture-design.md
//         docs/architecture/2026-08-02-remote-daemon-r1-plan.md (R1 Task 9)
//
// no-port-check: NereusSDR-original. This is the daemon's own entry point;
// there is no Thetis equivalent (Thetis is GUI-only). It links NereusCore
// alone -- no NereusGui, and therefore no GUI object code at all -- so it
// can run headless on a Pi with no display and no sound card.
//
// It does NOT follow that no Qt GUI module is on the link line. NereusCore
// links Qt6::Widgets PUBLIC, so nereusd links Qt6::Widgets and Qt6::Gui
// transitively; see nereus_apply_core_deps() in CMakeLists.txt for why
// that link is retained (the precompiled header NereusCore and NereusGui
// share includes <QWidget> / <QPainter>). An earlier version of this
// comment claimed "no Qt Widgets, no QRhi", which was never true of the
// link line. What is true, and what actually matters, is that no widget
// is constructed and no GUI symbol is referenced.
//
// tests/tst_core_has_no_gui_includes enforces the invariant this depends
// on: src/core and src/models never #include src/gui, nor any QtWidgets /
// QtQuick / QtGui-rendering / QRhi header. The nereusd CMake target
// additionally proves the NereusGui half at link time, since NereusGui is
// never named on nereusd's link line.
//
// R1 Task 8 left two decisions for this task (see src/main.cpp's own
// "R1 Task 9 candidate" comments and task-9-report.md for the full
// reasoning):
//
//   1. qRegisterMetaType<RadioConnectionError>/<AudioDeviceConfig>:
//      registered again here rather than moved out of main.cpp or folded
//      into CoreInit. Neither type is exercised by anything this task
//      builds (RadioConnection/AudioEngine are not constructed until R1
//      Task 10's DaemonApp), but the registration is two harmless lines
//      and closing it now avoids a silent cross-thread signal-delivery gap
//      the moment Task 10 lands. This is the SECOND call site (after
//      main.cpp's); CoreInit's own Task 8 report reserved centralising the
//      registration for when a THIRD appears.
//   2. SIGTERM/SIGINT: the handler only sets a sig_atomic_t flag. A timer
//      on the application thread calls quit() after observing it. Posting
//      a queued Qt call from a signal handler allocates and takes locks,
//      which can deadlock if the signal interrupted either operation.
//
// R1 Task 10, fix round 1, Finding 3: daemon.start(cfg) is scheduled via
// a queued QMetaObject::invokeMethod rather than called inline before
// app.exec() below. Reachability matters here in a way it did not
// before this task: before Task 10, this file never constructed a
// RadioModel at all, so nothing here could ever run
// RadioModel::connectToRadio()'s cold-cache WDSP wisdom wait (a
// synchronous nested QEventLoop that can block for many minutes -- see
// DaemonApp.h). Calling start() inline meant that wait was the ONLY
// event loop alive during a cold-cache first connect: app.exec() was
// never reached, so the original queued quit had no outer loop to land
// on, and a SIGTERM
// arriving in that window could not be serviced -- confirmed live
// (task-10-report.md): SIGTERM sent mid-wisdom-generation did not
// unwind within 10 seconds and required SIGKILL. Deferring start() to
// run AFTER app.exec() begins means the SAME nested QEventLoop
// (RadioModel.cpp) is now nested INSIDE a live outer loop, so the signal
// polling timer can request quit during that wait as well.
//
// One further reconciliation beyond the brief's own server_main.cpp
// sketch (task-9-brief.md Step 4): that sketch predates CoreInit::shutdown()
// (added by Task 8 itself, beyond its own brief's literal interface) and so
// never calls it. CoreInit::initialize() installs a custom Qt message
// handler and opens a log file; leaving the handler installed past
// QCoreApplication's own teardown is exactly the hazard shutdown() exists
// to close (see CoreInit.cpp's comment on QThreadStoragePrivate::finish).
// nereusd calls it for the same reason the GUI does.
//
// R1 Task 9 fix round 1: nereusd had no isolation mechanism analogous to
// main.cpp's --profile, and verifying the SIGTERM reconciliation above
// against the real binary touched JJ's real ~/Library/Preferences/
// NereusSDR/ (task-9-report.md section 5) -- the same directory the real
// GUI client uses, because CoreInit::initialize()'s AppSettings::instance()
// call resolves there with no override in place. This gap was already
// flagged in Task 8's own review, before Task 9 existed ("Note for Task
// 9's daemon caller"), but never reached this task's brief or dispatch.
// Closed here: --profile/-p, resolved through the SAME QCommandLineParser
// already built for --config -- no pre-QCoreApplication argv scan needed
// the way main.cpp's extractProfileFromArgv() is. main.cpp's scan exists
// because the GUI reads UiScalePercent from the settings file and sets
// QT_SCALE_FACTOR before constructing QApplication (Qt reads that
// environment variable at construction time); nereusd has no such
// constraint (QCoreApplication does not consult QT_SCALE_FACTOR at all),
// and nothing between this file's QCoreApplication construction and its
// AppSettings::setProfileOverride() call below touches AppSettings::
// instance() -- setApplicationName(), the signal handlers, the two
// qRegisterMetaType calls, and QCommandLineParser's own construction/
// addOption/process are all pure Qt-or-libc operations with no reach into
// our AppSettings class. AppSettings::instance() is first touched inside
// CoreInit::initialize(), which runs after the profile is resolved and
// (if valid) already pinned, so the ordinary post-construction parser path
// is sufficient. See DaemonConfig.h's resolveDaemonProfileArgument() for
// why an invalid name is fatal here rather than a silent fallback to the
// shared directory the way a mistyped GUI --profile is.
// =================================================================
// Modification history (NereusSDR):
//   2026-08-02: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-20: make termination signal handling allocation-free.
//               J.J. Boyd (KG4VCF), with AI assistance via OpenAI Codex.
//   2026-09-23: place signal processing threads on the fastest cores
//               before any other thread starts (R-R3-41). J.J. Boyd
//               (KG4VCF), with AI assistance via Anthropic Claude Code.
//   2026-09-24: --test-link-majors, debug builds only (iPhone app Task 4,
//               R-IOS-01). J.J. Boyd (KG4VCF), with AI assistance via
//               Anthropic Claude Code.
//   2026-09-24: console subcommands (status, pairing, devices, token,
//               reset) sent to the running Core over its control socket
//               (iPhone app Task 17, R-IOS-08). J.J. Boyd (KG4VCF), with AI
//               assistance via Anthropic Claude Code.
//   2026-09-24: Part C fix wave (R2-M6): console commands report
//               an unreadable or invalid configuration file. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic Claude
//               Code.
//   2026-09-26: console commands also look in a packaged Core's HOME, so
//               sudo nereusd <command> reaches it with no other options
//               (R-IOS-08, R-R3-26). J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-29: read the opt-in ICE check log switch (NEREUS_ICE_DIAG)
//               before any peer or log line exists. J.J. Boyd (KG4VCF),
//               with AI assistance via Anthropic Claude Code.
// =================================================================

#include "core/AppSettings.h"
#include "core/AudioDeviceConfig.h"
#include "core/CoreInit.h"
#include "core/LogCategories.h"
#include "core/RadioConnection.h"
#include "core/BuildIdentity.h"
#include "core/daemon/DaemonApp.h"
#include "core/daemon/DaemonConfig.h"
#include "core/daemon/StationControlCommands.h"
#include "core/daemon/StationControlSocket.h"
#include "core/platform/ThreadPlacement.h"
#include "core/session/LinkVersion.h"
#include "core/station/StationHandover.h"
#ifdef NEREUS_BUILD_TESTS
#include <QDir>
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#endif

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QMetaObject>
#include <QLocalSocket>
#include <QPointer>
#include <QTimer>
#include <csignal>
#include <cstdio>
#include <memory>

#include "core/session/IceDiagnostics.h"

// A few entry-point tests include this file directly. Only the actual
// nereusd target receives the generated-header include directory.
#ifdef NEREUS_DAEMON_EXECUTABLE
#include "NereusBuildTag.h"
#endif

namespace {

QCoreApplication* s_app = nullptr;
volatile std::sig_atomic_t s_terminationRequested = 0;

// No Qt, allocation, or locks are permitted in signal context. Keep the
// flag set once requested so another signal cannot race a reset.
void onTerm(int)
{
    s_terminationRequested = 1;
}

class ReleaseCoordinator final : public QObject {
public:
    ReleaseCoordinator(NereusSDR::DaemonApp& daemon, NereusSDR::StationHandover& ownership,
                       QCoreApplication& app)
        : m_daemon(daemon), m_ownership(ownership), m_app(app)
    {
#ifdef NEREUS_BUILD_TESTS
        m_testReleaseDelayMs = qMax(0, qEnvironmentVariableIntValue(
            "NEREUS_HANDOVER_TEST_RELEASE_DELAY_MS"));
        m_testRecoveryDelayMs = qMax(0, qEnvironmentVariableIntValue(
            "NEREUS_HANDOVER_TEST_RECOVERY_DELAY_MS"));
#endif
    }

    bool listen(const QString& path)
    {
        m_control = std::make_unique<NereusSDR::StationControlSocket>(
            [this](const QStringList& args) {
                if (m_state != State::Running
                    && args.value(0) != QLatin1String("status")) {
                    return NereusSDR::StationControlReply{
                        false, QStringLiteral("The Core is handing back the radio. Only status "
                                              "and release are available now.")};
                }
                return m_daemon.runControlCommand(args);
            }, this,
            [this](const QStringList& args, QLocalSocket* socket) {
                if (args != QStringList{QStringLiteral("release")}) { return false; }
                acceptRelease(socket);
                return true;
            });
        return m_control->listen(path);
    }

    void close() { if (m_control) { m_control->close(); } }
    QString lastError() const { return m_control ? m_control->lastError() : QString(); }

private:
    enum class State { Running, Releasing, Recovering, Failed, Released };

    void acceptRelease(QLocalSocket* socket)
    {
        if (m_state == State::Releasing || m_state == State::Recovering) {
            NereusSDR::StationControlSocket::sendReply(
                socket, {false, QStringLiteral("The Core is already handing back the radio.")});
            return;
        }
        if (m_state == State::Released) {
            NereusSDR::StationControlSocket::sendReply(
                socket, {true, QStringLiteral("The Core handed back the radio.")});
            return;
        }
        m_state = State::Releasing;
        m_replySocket = socket;
        m_elapsed.start();
        // Freeze console mutations before stopAllTx can emit a direct
        // callback and re-enter the local event loop. stopAllTx remains the
        // first operation on station/radio state in beginStationRelease().
        m_daemon.beginStationRelease();
        QTimer::singleShot(0, this, [this]() { advance(); });
    }

    void fail(const QString& reason)
    {
        // Keep every control mutation and duplicate release fenced until the
        // pre-teardown owner is listening again. A late save failure has no
        // model to restore and settles in Failed instead.
        m_state = State::Recovering;
        if (m_replySocket) {
            NereusSDR::StationControlSocket::sendReply(
                m_replySocket, {false, reason.isEmpty()
                    ? QStringLiteral("The Core could not hand back the radio. Try release again.")
                    : reason});
        }
        m_replySocket.clear();
        QTimer::singleShot(m_testRecoveryDelayMs, this, [this]() { advanceRecovery(); });
    }

    void advanceRecovery()
    {
        if (m_state != State::Recovering) { return; }
        const auto result = m_daemon.recoverFailedStationRelease();
        if (result == NereusSDR::DaemonApp::StationReleaseRecoveryResult::Pending) {
            QTimer::singleShot(50, this, [this]() { advanceRecovery(); });
            return;
        }
        m_state = result == NereusSDR::DaemonApp::StationReleaseRecoveryResult::Restored
            ? State::Running : State::Failed;
    }

    void advance()
    {
        if (m_state != State::Releasing) { return; }
        // Leave room inside the console's existing 15-second wait for a
        // truthful failure reply and its socket drain.
        if (m_elapsed.elapsed() >= 12000) {
            fail(QStringLiteral("The Core did not finish handing back the radio in time. "
                                "It still owns the radio; try release again."));
            return;
        }
#ifdef NEREUS_BUILD_TESTS
        if (m_elapsed.elapsed() < m_testReleaseDelayMs) {
            QTimer::singleShot(50, this, [this]() { advance(); });
            return;
        }
#endif
        QString reason;
        const auto result = m_daemon.tryCompleteStationRelease(&reason);
        if (result == NereusSDR::DaemonApp::StationReleaseResult::Pending) {
            QTimer::singleShot(50, this, [this]() { advance(); });
            return;
        }
        if (result == NereusSDR::DaemonApp::StationReleaseResult::Failed) {
            fail(reason);
            return;
        }

        // No model or settings writer remains. Close and remove the old
        // listener pathname while we still own the profile. The accepted
        // socket was reparented off QLocalServer and stays alive for reply.
        NereusSDR::CoreInit::shutdown();
        m_control->close();
        m_ownership.release();
        m_state = State::Released;
        if (m_replySocket) {
            QLocalSocket* const socket = m_replySocket;
            QCoreApplication* const application = &m_app;
            QObject::connect(socket, &QLocalSocket::disconnected, &m_app,
                             [application]() { application->exit(0); });
            if (NereusSDR::StationControlSocket::sendReply(
                    socket, {true, QStringLiteral("The Core handed back the radio.")})) {
                // A peer that disappears must not keep a released, headless
                // process alive indefinitely. Normal path exits on drain.
                QTimer::singleShot(2000, &m_app, [application]() { application->exit(0); });
                return;
            }
        }
        QCoreApplication* const application = &m_app;
        QTimer::singleShot(0, &m_app, [application]() { application->exit(0); });
    }

    NereusSDR::DaemonApp& m_daemon;
    NereusSDR::StationHandover& m_ownership;
    QCoreApplication& m_app;
    std::unique_ptr<NereusSDR::StationControlSocket> m_control;
    QPointer<QLocalSocket> m_replySocket;
    QElapsedTimer m_elapsed;
    State m_state {State::Running};
    int m_testRecoveryDelayMs {0};
#ifdef NEREUS_BUILD_TESTS
    int m_testReleaseDelayMs {0};
#endif
};

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("nereusd"));
    QCoreApplication::setApplicationVersion(QStringLiteral(NEREUSSDR_VERSION));
#ifdef NEREUSSDR_BUILD_TAG
    NereusSDR::BuildIdentity::setBuildTag(QString::fromUtf8(NEREUSSDR_BUILD_TAG));
#else
    NereusSDR::BuildIdentity::setBuildTag({});
#endif
    s_app = &app;

    std::signal(SIGTERM, onTerm);
    std::signal(SIGINT,  onTerm);

    QTimer terminationPoll;
    QObject::connect(&terminationPoll, &QTimer::timeout, &app, [&app]() {
        if (s_terminationRequested) {
            app.quit();
        }
    });
    terminationPoll.start(50);

    // Register custom metatypes for cross-thread signal/slot connections.
    // See the file header above (Task 8 deferral 1) for why these are
    // registered here rather than moved or centralised.
    qRegisterMetaType<NereusSDR::RadioConnectionError>();
    qRegisterMetaType<NereusSDR::AudioDeviceConfig>();

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("NereusSDR headless daemon"));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption buildInfoOpt(QStringLiteral("build-info"),
        QStringLiteral("Print this Core executable's product version and source tag as JSON."));
    parser.addOption(buildInfoOpt);
    QCommandLineOption cfgOpt({QStringLiteral("c"), QStringLiteral("config")},
        QStringLiteral("Config file path."), QStringLiteral("path"),
        QStringLiteral("/etc/nereusd.conf"));
    parser.addOption(cfgOpt);
    QCommandLineOption profileOpt({QStringLiteral("p"), QStringLiteral("profile")},
        QStringLiteral(
            "Run against an isolated settings/log profile instead of the "
            "shared default directory. Lets two nereusd instances on the "
            "same machine (or a developer workstation that also runs the "
            "GUI client) avoid clobbering each other's state. Name must "
            "match [A-Za-z0-9_-]+."),
        QStringLiteral("name"));
    parser.addOption(profileOpt);
    // iPhone app Task 4 (R-IOS-01): replaces the link majors the station
    // advertises and accepts, so the app's version screens can be tried
    // against a real station. Accepted by a debug build only; a release
    // build refuses to start with it (LinkVersion::resolveTestLinkMajors).
    QCommandLineOption testLinkMajorsOpt(QStringLiteral("test-link-majors"),
        QStringLiteral("Debug builds only: the link versions this Core offers, "
                       "for example 1,2."),
        QStringLiteral("list"));
    parser.addOption(testLinkMajorsOpt);
    // iPhone app Task 17 (R-IOS-08): a command word makes this process a
    // console command for the Core already running with the same --config
    // and --profile, instead of a second Core. No command runs the Core.
    parser.addPositionalArgument(QStringLiteral("command"),
        QStringLiteral("Optional. A command for the running Core: status, pairing show|open|close, "
                       "devices, devices revoke <id>, token retire, reset --unclaimed --yes, "
                       "release."),
        QStringLiteral("[command...]"));
    QCommandLineOption unclaimedOpt(QStringLiteral("unclaimed"),
        QStringLiteral("With reset: return the Core to having no paired device."));
    parser.addOption(unclaimedOpt);
    QCommandLineOption yesOpt(QStringLiteral("yes"),
        QStringLiteral("With reset: go ahead without asking."));
    parser.addOption(yesOpt);
    parser.process(app);
    if (parser.isSet(buildInfoOpt)) {
        const QJsonObject identity{
            {QStringLiteral("productVersion"), QCoreApplication::applicationVersion()},
            {QStringLiteral("sourceTag"), NereusSDR::BuildIdentity::buildTag()}};
        const QByteArray json = QJsonDocument(identity).toJson(QJsonDocument::Compact) + '\n';
        std::fwrite(json.constData(), 1, static_cast<size_t>(json.size()), stdout);
        std::fflush(stdout);
        return 0;
    }

    const QStringList commandWords = parser.positionalArguments();
    if (!commandWords.isEmpty()) {
        // Nothing here touches AppSettings, the log or the Core's files: the
        // socket's place comes from the config file (/etc/nereusd.conf
        // unless --config names another) and the profile name
        // (StationControlSocket::socketPathFor). Without state_directory
        // that is the profile's directory under this command's $HOME, and
        // then under a packaged Core's HOME, /var/lib/nereusd, since under
        // sudo $HOME is root's (StationControlSocket::candidatePathsFor).
        const auto print = [](FILE* stream, const QString& text) {
            const QByteArray bytes = (text + QLatin1Char('\n')).toUtf8();
            std::fwrite(bytes.constData(), 1, static_cast<size_t>(bytes.size()), stream);
            std::fflush(stream);
        };
        if (!NereusSDR::StationControlCommands::isCommand(commandWords.first())) {
            print(stderr, QStringLiteral("Unknown command.\n")
                              + NereusSDR::StationControlCommands::usage());
            return 2;
        }
        QString commandProfileErr;
        const QString commandProfile = NereusSDR::resolveDaemonProfileArgument(
            parser.value(profileOpt), parser.isSet(profileOpt), &commandProfileErr);
        if (!commandProfileErr.isEmpty()) {
            print(stderr, commandProfileErr);
            return 2;
        }
        QString commandCfgErr;
        const NereusSDR::DaemonConfig commandCfg =
            NereusSDR::DaemonConfig::fromFile(parser.value(cfgOpt), &commandCfgErr);
        // Part C fix wave (R2-M6): a configuration file that is there but
        // cannot be read (sudo left off, say), or one the Core itself would
        // refuse to start with, is reported, not left to look like "No Core
        // answered" from the wrong socket. An absent file is what a bare
        // nereusd runs with too: defaults, on both sides.
        if (!commandCfgErr.isEmpty() && QFileInfo::exists(parser.value(cfgOpt))) {
            print(stderr, QStringLiteral("Could not read the configuration file %1, so the Core "
                                         "cannot be found. Run the command with sudo, or name "
                                         "another file with --config.")
                              .arg(parser.value(cfgOpt)));
            return 2;
        }
        QString commandCfgInvalid;
        if (!commandCfg.validate(&commandCfgInvalid)) {
            print(stderr, QStringLiteral("The configuration file %1 is not valid, so the Core "
                                         "does not start with it: %2")
                              .arg(parser.value(cfgOpt), commandCfgInvalid));
            return 2;
        }
        QStringList args = commandWords;
        if (parser.isSet(unclaimedOpt)) {
            args << QStringLiteral("--unclaimed");
        }
        if (parser.isSet(yesOpt)) {
            args << QStringLiteral("--yes");
        }
        const NereusSDR::StationControlReply reply = NereusSDR::StationControlSocket::request(
            NereusSDR::StationControlSocket::candidatePathsFor(commandCfg, commandProfile), args);
        print(reply.ok ? stdout : stderr, reply.text);
        return reply.ok ? 0 : 1;
    }

    QString linkMajorsErr;
    const QList<quint16> linkMajors = NereusSDR::LinkVersion::resolveTestLinkMajors(
        parser.isSet(testLinkMajorsOpt), parser.value(testLinkMajorsOpt),
        NereusSDR::LinkVersion::testLinkMajorsAllowed(), &linkMajorsErr);
    if (linkMajors.isEmpty()) {
        qCCritical(NereusSDR::lcApp).noquote() << linkMajorsErr;
        return 3;
    }
    if (parser.isSet(testLinkMajorsOpt)) {
        qCWarning(NereusSDR::lcApp) << "Test link versions in force:" << linkMajors;
    }

    // Resolve and validate --profile BEFORE CoreInit::initialize(), which
    // is where AppSettings::instance() is first touched in this process
    // (see the file header above for why the ordinary post-construction
    // parser path is sufficient here, unlike main.cpp). An invalid name is
    // fatal: unlike main.cpp, which warns and falls back to the shared
    // directory, a daemon provisioning mistake should stop the daemon
    // rather than silently share state with something else on the box.
    //
    // Remote Daemon R2, Task 1: `profileWasSet` (parser.isSet(), not
    // parser.value()) is what lets resolveDaemonProfileArgument() tell "no
    // --profile at all" apart from "--profile with an empty value" -- both
    // are "" out of parser.value() once profileOpt above is declared with
    // no default. The resolved profile string is threaded to BOTH
    // AppSettings::setProfileOverride() below AND CoreInit::initialize()
    // a few lines down, which is what keeps the log directory (resolved
    // independently inside CoreInit.cpp via AppSettings::resolveConfigDir())
    // moving together with the settings path instead of the two drifting
    // apart. See DaemonConfig.h's resolveDaemonProfileArgument() for the
    // full rationale.
    QString profileErr;
    const bool profileWasSet = parser.isSet(profileOpt);
    const QString profile = NereusSDR::resolveDaemonProfileArgument(
        parser.value(profileOpt), profileWasSet, &profileErr);
    if (!profileErr.isEmpty()) {
        qCCritical(NereusSDR::lcApp) << profileErr;
        return 3;
    }
    if (!profile.isEmpty()) {
        NereusSDR::AppSettings::setProfileOverride(profile);
    }

    // Every same-profile process serializes settings, identity and radio
    // ownership before CoreInit loads or writes anything in that profile.
    NereusSDR::StationHandover ownership(profile);
    QString lockError;
    if (!ownership.acquire(15000, &lockError)) {
        qCCritical(NereusSDR::lcApp).noquote() << lockError;
        return 1;
    }

    // Task 1: warn, never fail, when the resolved profile still lands on
    // the same settings/log directory the GUI client uses. Sharing stays
    // a supported configuration -- an operator opts into it explicitly
    // with --profile "" -- but a collision is worth flagging loudly
    // either way, the same way a mistyped --profile is (above), just
    // non-fatally.
    {
        const QString resolvedDir = NereusSDR::AppSettings::resolveConfigDir(profile);
        const QString sharedDir = NereusSDR::AppSettings::resolveConfigDir(QString());
        if (resolvedDir == sharedDir) {
            const char* producedBy = profileWasSet
                ? "an explicitly empty --profile"
                : "the reserved default profile (unexpected)";
            qCWarning(NereusSDR::lcApp)
                << "nereusd settings/log directory" << resolvedDir
                << "is the SAME directory the GUI client uses" << sharedDir
                << "-- produced by" << producedBy;
        }
    }

    if (!NereusSDR::CoreInit::initialize(profile)) {
        qCCritical(NereusSDR::lcApp) << "core initialization failed";
        return 1;
    }

    // Task 1: mark this settings store as having seen a nereusd first run,
    // so R2 Task 15's Setup gate can later tell "this profile's settings
    // snapshot is empty because it is a legitimately fresh daemon
    // profile" apart from "the snapshot is empty because something is
    // broken". Idempotent -- see AppSettings::seedDaemonProfileMarker().
    NereusSDR::AppSettings::instance().seedDaemonProfileMarker();

    QString err;
    const NereusSDR::DaemonConfig cfg =
        NereusSDR::DaemonConfig::fromFile(parser.value(cfgOpt), &err);
    if (!err.isEmpty()) {
        qCWarning(NereusSDR::lcApp) << "config:" << err << "- continuing with defaults";
    }
    if (!cfg.validate(&err)) {
        qCCritical(NereusSDR::lcApp) << "invalid config:" << err;
        return 2;
    }

    // R-R3-41: settings are initialised and no other thread exists yet, so
    // this moves the main thread to the housekeeping cores and every thread
    // started from now on (WDSP, networking, spectrum, libdatachannel)
    // starts there. Only signal processing threads move to their own cores.
    NereusSDR::startDaemonThreadPlacement(cfg.threadPlacement, cfg.sliceCount);

    // "requested" on both counts, deliberately. Neither value is final
    // here: sliceCount is clamped to the connected board's maxSlices by
    // DaemonApp, and sampleRateHz is seeded into the per-MAC settings key
    // and then validated against the board's allowed-rate list by
    // resolveSampleRate(), which logs the rate it actually connects with
    // ("Connecting with sampleRate=" in RadioModel). An earlier version
    // of this line printed the rate as a bare "rate", which read as
    // "applied" for a value that at the time reached nothing but a
    // test-only branch.
    qCInfo(NereusSDR::lcApp) << "nereusd starting, requested slices"
                             << cfg.sliceCount
                             << "requested rate" << cfg.sampleRateHz;

    // The opt-in ICE check log must own libdatachannel's logger before the
    // first peer exists; each peer takes its log level when it is created.
    NereusSDR::IceDiagnostics::installFromEnvironment();

    // `daemon` is declared after `app` (QCoreApplication), so C++ runs
    // its destructor before app's when main() returns -- teardown still
    // has a live QCoreApplication to run on. The explicit stop() call on
    // the quit path below runs that same teardown earlier, and visibly,
    // rather than relying solely on the implicit destructor call.
    NereusSDR::DaemonApp daemon;
    daemon.setLinkMajors(linkMajors);
#ifdef NEREUS_BUILD_TESTS
    if (qEnvironmentVariableIsSet("NEREUS_HANDOVER_TEST_DISABLE_DISCOVERY")) {
        // Exercise a genuinely pending layout without LAN discovery.
        daemon.setDiscoveryProviderForTest([] { return QList<NereusSDR::RadioInfo>{}; });
    }
    if (qEnvironmentVariableIsSet("NEREUS_HANDOVER_TEST_PRIMED_BOARD")) {
        // Process integration tests must never send radio discovery packets.
        daemon.primeBoardForTest(NereusSDR::HPSDRHW::HermesLite,
                                 QStringLiteral("02:00:00:00:00:48"));
    }
    if (qEnvironmentVariableIsSet("NEREUS_HANDOVER_TEST_FAIL_AFTER_STOP")) {
        // Force only the final settings write to fail, after stop() has
        // destroyed the model. A directory at the destination defeats
        // QSaveFile even under uid 0. The test parent restores the original
        // file in this isolated profile before asking the owner to retry.
        const QString settingsPath = NereusSDR::AppSettings::instance().filePath();
        daemon.setAfterStationStopForTest([settingsPath, first = true]() mutable {
            if (!first) { return; }
            first = false;
            const QString preserved = settingsPath
                + QStringLiteral(".handover-test-original");
            if (!QFileInfo(settingsPath).isFile() || QFileInfo::exists(preserved)
                || !QFile::rename(settingsPath, preserved)) {
                qCCritical(NereusSDR::lcApp) << "test could not preserve settings"
                                            << settingsPath;
                return;
            }
            if (!QDir().mkdir(settingsPath)) {
                QFile::rename(preserved, settingsPath);
                qCCritical(NereusSDR::lcApp) << "test could not block settings save"
                                            << settingsPath;
            }
        });
    }
#endif
    ReleaseCoordinator release(daemon, ownership, app);

    // R1 Task 10: connects to a radio (or runs discovery when
    // cfg.radioMac is empty) and creates min(cfg.sliceCount,
    // connected-board-maxSlices) slices. See DaemonApp.h for why this
    // returns true even when no radio was found at startup -- the same
    // reason the equally-unconditional CoreInit::initialize() check
    // above exists: a documented bool return value gets checked
    // regardless of whether today's implementation can currently return
    // false.
    //
    // Fix round 1, Finding 3: scheduled via a queued invokeMethod rather
    // than called inline here, so app.exec() below is already running
    // by the time it executes -- see the file header above for why that
    // ordering is load-bearing, not cosmetic. `daemon` and `cfg` are
    // captured by reference: both are local to this function and stay
    // alive for the rest of main(), well past the point this queued call
    // runs (the queued event is serviced from the very first turn of
    // app.exec()'s loop, still inside this stack frame).
    QMetaObject::invokeMethod(s_app, [&daemon, &cfg, &profile, &release]() {
        if (!daemon.start(cfg)) {
            qCCritical(NereusSDR::lcApp) << "daemon failed to start";
            QCoreApplication::exit(4);
            return;
        }
#ifdef NEREUS_BUILD_TESTS
        if (qEnvironmentVariableIsSet("NEREUS_HANDOVER_TEST_DIRTY_OFFLINE")) {
            // Test-only post-baseline edit while discovery is disabled.
            if (auto* model = daemon.radioModelForTest()) {
                if (auto* slice = model->sliceById(0)) {
                    slice->setAfGain(slice->afGain() + 1);
                }
            }
        }
#endif
        qCInfo(NereusSDR::lcApp) << "nereusd started, slices" << daemon.sliceCount();
        // iPhone app Task 17: the console commands reach this Core through
        // state_directory, or the profile's directory.
        if (!release.listen(NereusSDR::StationControlSocket::socketPathFor(cfg, profile))) {
            qCWarning(NereusSDR::lcApp) << "The Core's console socket could not listen:"
                                        << release.lastError();
        }
    }, Qt::QueuedConnection);

    const int rc = app.exec();

    // Quit path: disconnect the radio and drop every slice before
    // CoreInit::shutdown() below. Safe even if start() somehow left
    // nothing to tear down (DaemonApp::stop() is safe with no prior
    // start()).
    daemon.stop();
    release.close();

    // Mirrors main.cpp's teardown: uninstalls CoreInit::initialize()'s
    // message handler and closes its log file before static destructors
    // start running. See the file header above and CoreInit.cpp.
    NereusSDR::CoreInit::shutdown();
    return rc;
}
