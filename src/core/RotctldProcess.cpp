// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// src/core/RotctldProcess.cpp  (NereusSDR)
// =================================================================
//
// Ported from Longpath source:
//   src/core/RotctldProcess.cpp [@551576e], original header from Longpath
//   source is included below.
//
// Longpath (Martin Fischer, OE5SOS, https://github.com/oe5sos/Longpath)
// is a fork of NereusSDR distributed under the GNU General Public
// License version 3 (its root LICENSE). Upstream source has no
// top-of-file GPL header; project-level LICENSE applies.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-08: Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted transformation via Anthropic
//               Claude Code. Rotor control plan, Task 3b. See
//               RotctldProcess.h for what changed.
//   2026-10-08: Final review fixes: a start that fails is said in plain
//               words (startFailedReason), Qt's text logged. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-08: Re-review N4: the test override is read and written under
//               a lock, as the host scan reads it on a pool thread.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================
//
// --- From RotctldProcess.cpp ---
//
// =================================================================
// src/core/RotctldProcess.cpp  (Longpath)
// =================================================================
//
// Longpath-original — see RotctldProcess.h.
//
// =================================================================
// Modification history (Longpath):
//   2026-08-07 — Created in C++20/Qt6 for NereusSDR, AI-assisted via
//                 Anthropic Claude (Cowork), operator Martin Fischer.
// =================================================================

#include "core/RotctldProcess.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QHostAddress>
#include <QLoggingCategory>
#include <QMutex>
#include <QStandardPaths>
#include <QTcpServer>

namespace NereusSDR {

namespace {

// From Longpath src/core/RotctldProcess.cpp:25 [@551576e]
Q_LOGGING_CATEGORY(lcRotctld, "nereus.rotctld")

// Final review M8: why rotctld would not start, in the operator's words.
QString startFailedReason(QProcess::ProcessError error)
{
    if (error == QProcess::FailedToStart) {
        return QStringLiteral("Hamlib's rotctld would not start. Check that Hamlib is "
                              "installed and this computer's account may run it.");
    }
    return QStringLiteral("Hamlib's rotctld would not start. Check the rotor's port and "
                          "model in Setup.");
}

// findBinary()'s test override (setBinaryOverrideForTesting). The host
// scan calls findBinary() on a pool thread while a test may set or reset
// the override, so it is read and written under a lock (re-review N4).
QMutex& binaryOverrideLock()
{
    static QMutex lock;
    return lock;
}
std::optional<QString>& binaryOverride()
{
    static std::optional<QString> path;
    return path;
}

// From Longpath src/core/RotctldProcess.cpp:27-34 [@551576e]
// True if nothing on this machine is listening on loopback:port. A
// bind that succeeds is released again at once; rotctld binds it for
// real a moment later.
bool loopbackPortIsFree(quint16 port)
{
    QTcpServer probe;
    return probe.listen(QHostAddress::LocalHost, port);
}

// From Longpath src/core/RotctldProcess.cpp:36-42 [@551576e]
// A port the kernel says is free right now.
quint16 anyFreeLoopbackPort()
{
    QTcpServer probe;
    if (!probe.listen(QHostAddress::LocalHost, 0)) { return 0; }
    return probe.serverPort();
}

} // namespace

QString RotctldProcess::notInstalledReason()
{
    return QStringLiteral(
        "Hamlib's rotctld is not installed on the Core's computer.");
}

// From Longpath src/core/RotctldProcess.cpp:46-60 [@551576e]
RotctldProcess::RotctldProcess(QObject* parent) : QObject(parent)
{
    connect(&m_proc, &QProcess::finished, this,
            [this](int code, QProcess::ExitStatus) {
        const QString err =
            QString::fromLocal8Bit(m_proc.readAllStandardError()).trimmed();
        if (m_stopRequested) {
            m_stopRequested = false;
            return;
        }
        qCWarning(lcRotctld) << "rotctld exited on its own, code" << code
                             << err;
        emit exited(code, err);
    });

    // NereusSDR: stop on the way out even when the owner outlives the
    // event loop, so a quitting Core never leaves rotctld holding the
    // serial port.
    if (QCoreApplication* app = QCoreApplication::instance()) {
        connect(app, &QCoreApplication::aboutToQuit,
                this, &RotctldProcess::stop);
    }
}

// From Longpath src/core/RotctldProcess.cpp:62-106 [@551576e]
QString RotctldProcess::reasonFromStderr(const QString& stderrText)
{
    // Begruendung im Kopf. Hamlibs stderr ist ein Ablaufprotokoll; die
    // letzte Zeile ist die Folge, nicht die Ursache.
    static const QStringList kCauses = {
        QStringLiteral("timed out"), QStringLiteral("refused"),
        QStringLiteral("no such"),   QStringLiteral("in use"),
        QStringLiteral("permission"),QStringLiteral("not permitted"),
        QStringLiteral("unavailable"),
    };
    static const QStringList kFailures = {
        QStringLiteral("failed"), QStringLiteral("error"),
        QStringLiteral("cannot"), QStringLiteral("unable"),
    };

    const QStringList lines = stderrText.split(QLatin1Char('\n'));
    QString cause;
    QString failure;
    QString last;
    for (const QString& raw : lines) {
        const QString line = raw.trimmed();
        if (line.isEmpty()) { continue; }
        last = line;
        for (const QString& w : kCauses) {
            if (line.contains(w, Qt::CaseInsensitive)) { cause = line; break; }
        }
        // Eine Zeile zaehlt nur als Auskunft, wenn sie auch etwas BENENNT:
        // einen Rechner, einen Port, einen Pfad, eine Nummer. Der Pruefstand
        // hat diese Bedingung erzwungen -- "IO error" steht bei Hamlib immer
        // zuletzt und enthaelt das Wort "error", also gewann es jedes Mal
        // gegen die Zeile, die tatsaechlich etwas sagte.
        bool names = line.contains(QLatin1Char(':'));
        if (!names) {
            for (const QChar c : line) {
                if (c.isDigit()) { names = true; break; }
            }
        }
        if (names) {
            for (const QString& w : kFailures) {
                if (line.contains(w, Qt::CaseInsensitive)) { failure = line; break; }
            }
        }
    }
    if (!cause.isEmpty())   { return cause; }
    if (!failure.isEmpty()) { return failure; }
    return last;
}

// From Longpath src/core/RotctldProcess.cpp:108-111 [@551576e]
RotctldProcess::~RotctldProcess()
{
    stop();
}

void RotctldProcess::setBinaryOverrideForTesting(std::optional<QString> path)
{
    const QMutexLocker locker(&binaryOverrideLock());
    binaryOverride() = std::move(path);
}

// From Longpath src/core/RotctldProcess.cpp:113-136 [@551576e]
QString RotctldProcess::findBinary()
{
    {
        const QMutexLocker locker(&binaryOverrideLock());
        if (binaryOverride().has_value()) { return *binaryOverride(); }
    }

    // PATH first: an operator who installed Hamlib somewhere unusual
    // and put it on PATH has already answered this question.
    const QString onPath = QStandardPaths::findExecutable(
        QStringLiteral("rotctld"));
    if (!onPath.isEmpty()) { return onPath; }

    // Then the places package managers actually put it. A GUI launched
    // from Finder does not inherit the shell's PATH, so Homebrew's
    // directories have to be named explicitly or Hamlib is invisible to
    // this program while being plainly present in the terminal.
    //
    // NereusSDR: the Core also runs on Linux (a Rock 5C with Armbian,
    // started by systemd with a short PATH), where the distribution's
    // hamlib-utils package puts rotctld in /usr/bin and a source build
    // in /usr/local/bin; Homebrew on Linux uses /home/linuxbrew.
    static const QStringList kDirs = {
        QStringLiteral("/opt/homebrew/bin"),   // Apple silicon Homebrew
        QStringLiteral("/usr/local/bin"),      // Intel Homebrew, and most else
        QStringLiteral("/opt/local/bin"),      // MacPorts
        QStringLiteral("/usr/bin"),
        QStringLiteral("/home/linuxbrew/.linuxbrew/bin"),
    };
    for (const QString& dir : kDirs) {
        const QString candidate = dir + QStringLiteral("/rotctld");
        if (QFileInfo(candidate).isExecutable()) { return candidate; }
    }
    return {};
}

// From Longpath src/core/RotctldProcess.cpp:138-163 [@551576e]
QStringList RotctldProcess::arguments(int hamlibModel, const QString& device,
                                      int baud, quint16 listenPort)
{
    QStringList args;
    args << QStringLiteral("-m") << QString::number(hamlibModel);

    // A network model such as Ether6 takes an address where a serial
    // model takes a device node; either way it is -r, and either way
    // an empty one means "let Hamlib use its default".
    if (!device.trimmed().isEmpty()) {
        args << QStringLiteral("-r") << device.trimmed();
    }
    if (baud > 0) {
        args << QStringLiteral("-s") << QString::number(baud);
    }

    // Loopback only. rotctld has no authentication of any kind, and a
    // rotator that anyone on the network can turn is a rotator that
    // will eventually be turned by someone else. An operator who wants
    // it reachable from another machine can run rotctld themselves and
    // point Longpath at it — that is a decision worth making
    // deliberately.
    args << QStringLiteral("-T") << QStringLiteral("127.0.0.1")
         << QStringLiteral("-t") << QString::number(listenPort);
    return args;
}

// From Longpath src/core/RotctldProcess.cpp:165-168 [@551576e]
bool RotctldProcess::isRunning() const
{
    return m_proc.state() != QProcess::NotRunning;
}

// From Longpath src/core/RotctldProcess.cpp:170-226 [@551576e]
bool RotctldProcess::start(int hamlibModel, const QString& device, int baud,
                           quint16 listenPort, QString* error)
{
    if (isRunning()) { return true; }

    const QString binary = findBinary();
    if (binary.isEmpty()) {
        if (error) { *error = notInstalledReason(); }
        return false;
    }

    m_model         = hamlibModel;
    m_device        = device;
    m_baud          = baud;
    m_preferredPort = listenPort;

    quint16 port = listenPort;
    if (!loopbackPortIsFree(port)) {
        const quint16 other = anyFreeLoopbackPort();
        qCWarning(lcRotctld)
            << "port" << port << "is already taken (a leftover rotctld?),"
            << "using" << other << "instead";
        port = other;
        if (port == 0) {
            if (error) {
                *error = QStringLiteral(
                    "Port %1 on the Core's computer is in use and no free "
                    "port could be found. A rotctld from an earlier session "
                    "may still be running; stop it and try again.")
                    .arg(listenPort);
            }
            return false;
        }
    }
    m_listenPort = port;

    m_proc.setProgram(binary);
    m_proc.setArguments(arguments(hamlibModel, device, baud, port));
    m_stopRequested = false;
    m_proc.start();

    if (!m_proc.waitForStarted(3000)) {
        // Final review M8: plain words for the operator, Qt's in the log.
        qCWarning(lcRotctld).noquote() << "rotctld would not start:" << m_proc.errorString();
        if (error) {
            *error = startFailedReason(m_proc.error());
        }
        return false;
    }
    qCInfo(lcRotctld) << "started" << binary << m_proc.arguments();
    return true;
}

// From Longpath src/core/RotctldProcess.cpp:228-236 [@551576e]
bool RotctldProcess::restart(QString* error)
{
    if (m_model == 0) {
        if (error) {
            *error = QStringLiteral("Hamlib's rotctld was never started.");
        }
        return false;
    }
    stop();
    return start(m_model, m_device, m_baud, m_preferredPort, error);
}

// From Longpath src/core/RotctldProcess.cpp:238-252 [@551576e]
void RotctldProcess::stop()
{
    if (!isRunning()) { return; }

    // Ask first. rotctld closes the serial port on SIGTERM; killed
    // outright it can leave the port held until the device is
    // re-plugged, and the next connection attempt then fails for a
    // reason that has nothing to do with the rotator.
    m_stopRequested = true;
    m_proc.terminate();
    if (!m_proc.waitForFinished(2000)) {
        m_proc.kill();
        m_proc.waitForFinished(1000);
    }
}

} // namespace NereusSDR
