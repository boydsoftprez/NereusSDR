// =================================================================
// src/server_main.cpp  (NereusSDR)
// =================================================================
// nereusd: headless NereusSDR daemon.
// Design: docs/architecture/2026-07-28-remote-daemon-architecture-design.md
//         docs/architecture/2026-08-02-remote-daemon-r1-plan.md (R1 Task 9)
//
// no-port-check: NereusSDR-original. This is the daemon's own entry point;
// there is no Thetis equivalent (Thetis is GUI-only). It links NereusCore
// alone -- no NereusGui, no Qt Widgets, no QRhi -- so it can run headless
// on a Pi with no display and no sound card. tests/tst_core_has_no_gui_includes
// enforces the invariant this depends on (src/core and src/models never
// #include src/gui); the nereusd CMake target additionally proves it at
// link time, since NereusGui is never named on nereusd's link line.
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
// One further reconciliation beyond the brief's own server_main.cpp
// sketch (task-9-brief.md Step 4): that sketch predates CoreInit::shutdown()
// (added by Task 8 itself, beyond its own brief's literal interface) and so
// never calls it. CoreInit::initialize() installs a custom Qt message
// handler and opens a log file; leaving the handler installed past
// QCoreApplication's own teardown is exactly the hazard shutdown() exists
// to close (see CoreInit.cpp's comment on QThreadStoragePrivate::finish).
// nereusd calls it for the same reason the GUI does.
// =================================================================
// Modification history (NereusSDR):
//   2026-08-02: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
// =================================================================

#include "core/AudioDeviceConfig.h"
#include "core/CoreInit.h"
#include "core/LogCategories.h"
#include "core/RadioConnection.h"
#include "core/daemon/DaemonConfig.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QMetaObject>
#include <csignal>

namespace {

QCoreApplication* g_app = nullptr;

// Async-signal-safe: only QCoreApplication::quit() is approximately safe
// to call from signal context (it just sets an atomic flag the event loop
// polls), and even that is routed through QMetaObject::invokeMethod with
// Qt::QueuedConnection so the actual call happens on the event-loop thread
// rather than whatever thread the signal was delivered to. Same pattern as
// src/main.cpp's SIGTERM/SIGINT handlers; see the file header above for why
// this file does not just call g_app->quit() directly.
void onTerm(int)
{
    if (g_app) {
        QMetaObject::invokeMethod(g_app, "quit", Qt::QueuedConnection);
    }
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("nereusd"));
    g_app = &app;

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
    parser.process(app);

    if (!NereusSDR::CoreInit::initialize()) {
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

    qCInfo(NereusSDR::lcApp) << "nereusd starting, slices" << cfg.sliceCount
                             << "rate" << cfg.sampleRateHz;

    const int rc = app.exec();

    // Mirrors main.cpp's teardown: uninstalls CoreInit::initialize()'s
    // message handler and closes its log file before static destructors
    // start running. See the file header above and CoreInit.cpp.
    NereusSDR::CoreInit::shutdown();
    return rc;
}
