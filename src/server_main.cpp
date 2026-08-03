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
//   2. SIGTERM/SIGINT: reconciled rather than reimplemented a third way.
//      main.cpp posts QCoreApplication::quit() via QMetaObject::invokeMethod
//      with Qt::QueuedConnection instead of calling quit() directly from
//      the handler, specifically so the call lands on the event-loop
//      thread regardless of which thread the signal was delivered to. That
//      property matters here too -- R1 Task 11 gives nereusd a second
//      (wideband FFT) thread -- so onTerm() below uses the same indirection
//      main.cpp does, adapted to this file's simpler global-pointer
//      structure (no pre-QApplication argv scan to share it with).
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
// never reached, so onTerm()'s QMetaObject::invokeMethod(s_app, "quit",
// Qt::QueuedConnection) had no outer loop to land on, and a SIGTERM
// arriving in that window could not be serviced -- confirmed live
// (task-10-report.md): SIGTERM sent mid-wisdom-generation did not
// unwind within 10 seconds and required SIGKILL. Deferring start() to
// run AFTER app.exec() begins means the SAME nested QEventLoop
// (RadioModel.cpp) is now nested INSIDE a live outer loop, so a queued
// quit() posted during the wait is serviced exactly like it would be
// for any other nested-loop wait in this codebase.
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
// =================================================================

#include "core/AppSettings.h"
#include "core/AudioDeviceConfig.h"
#include "core/CoreInit.h"
#include "core/LogCategories.h"
#include "core/RadioConnection.h"
#include "core/daemon/DaemonApp.h"
#include "core/daemon/DaemonConfig.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QMetaObject>
#include <csignal>

namespace {

QCoreApplication* s_app = nullptr;

// Async-signal-safe: only QCoreApplication::quit() is approximately safe
// to call from signal context (it just sets an atomic flag the event loop
// polls), and even that is routed through QMetaObject::invokeMethod with
// Qt::QueuedConnection so the actual call happens on the event-loop thread
// rather than whatever thread the signal was delivered to. Same pattern as
// src/main.cpp's SIGTERM/SIGINT handlers; see the file header above for why
// this file does not just call s_app->quit() directly.
void onTerm(int)
{
    if (s_app) {
        QMetaObject::invokeMethod(s_app, "quit", Qt::QueuedConnection);
    }
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("nereusd"));
    s_app = &app;

    std::signal(SIGTERM, onTerm);
    std::signal(SIGINT,  onTerm);

    // Register custom metatypes for cross-thread signal/slot connections.
    // See the file header above (Task 8 deferral 1) for why these are
    // registered here rather than moved or centralised.
    qRegisterMetaType<NereusSDR::RadioConnectionError>();
    qRegisterMetaType<NereusSDR::AudioDeviceConfig>();

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("NereusSDR headless daemon"));
    parser.addHelpOption();
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
    parser.process(app);

    // Resolve and validate --profile BEFORE CoreInit::initialize(), which
    // is where AppSettings::instance() is first touched in this process
    // (see the file header above for why the ordinary post-construction
    // parser path is sufficient here, unlike main.cpp). An invalid name is
    // fatal: unlike main.cpp, which warns and falls back to the shared
    // directory, a daemon provisioning mistake should stop the daemon
    // rather than silently share state with something else on the box.
    QString profileErr;
    const QString profile =
        NereusSDR::resolveDaemonProfileArgument(parser.value(profileOpt), &profileErr);
    if (!profileErr.isEmpty()) {
        qCCritical(NereusSDR::lcApp) << profileErr;
        return 3;
    }
    if (!profile.isEmpty()) {
        NereusSDR::AppSettings::setProfileOverride(profile);
    }

    if (!NereusSDR::CoreInit::initialize(profile)) {
        qCCritical(NereusSDR::lcApp) << "core initialisation failed";
        return 1;
    }

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

    // `daemon` is declared after `app` (QCoreApplication), so C++ runs
    // its destructor before app's when main() returns -- teardown still
    // has a live QCoreApplication to run on. The explicit stop() call on
    // the quit path below runs that same teardown earlier, and visibly,
    // rather than relying solely on the implicit destructor call.
    NereusSDR::DaemonApp daemon;

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
    QMetaObject::invokeMethod(s_app, [&daemon, &cfg]() {
        if (!daemon.start(cfg)) {
            qCCritical(NereusSDR::lcApp) << "daemon failed to start";
            QCoreApplication::exit(4);
            return;
        }
        qCInfo(NereusSDR::lcApp) << "nereusd started, slices" << daemon.sliceCount();
    }, Qt::QueuedConnection);

    const int rc = app.exec();

    // Quit path: disconnect the radio and drop every slice before
    // CoreInit::shutdown() below. Safe even if start() somehow left
    // nothing to tear down (DaemonApp::stop() is safe with no prior
    // start()).
    daemon.stop();

    // Mirrors main.cpp's teardown: uninstalls CoreInit::initialize()'s
    // message handler and closes its log file before static destructors
    // start running. See the file header above and CoreInit.cpp.
    NereusSDR::CoreInit::shutdown();
    return rc;
}
