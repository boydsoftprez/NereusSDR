#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// src/core/daemon/StationControlCommands.h  (NereusSDR)
// =================================================================
//
// What each console subcommand does (iPhone app plan Task 17, R-IOS-08;
// spec section 9 settles the names here). The running Core answers them
// over StationControlSocket; `nereusd <command>` prints the answer.
//
//   nereusd status                     the Core's label, radio, remote
//                                      access, paired devices, pairing and
//                                      status page
//   nereusd pairing show               whether pairing is open, and the code
//   nereusd pairing open               reopen pairing on a claimed Core for
//                                      one more device (PairingWindow::
//                                      reopen()), and show the code
//   nereusd pairing close              close a reopened window; an
//                                      unclaimed Core stays open
//   nereusd devices                    the paired devices, with the id
//                                      revoke takes
//   nereusd devices revoke <id>        remove one; its connection ends
//                                      (StationServer on
//                                      DeviceStore::deviceRemoved)
//   nereusd token retire               stop accepting the older pairing
//                                      token: refused until a device is
//                                      paired, as station.retireToken is
//                                      (StationDevicesFacade::retireToken)
//   nereusd reset --unclaimed --yes    every paired device removed (a
//                                      damaged list moved aside first), the
//                                      token retired, every connection
//                                      ended, pairing open unclaimed with a
//                                      new code. Without --yes it changes
//                                      nothing and says what it would do.
//
// With remote access off (remote_port = 0) the Core has no pairing and no
// paired devices; the pairing, devices, token and reset commands say so
// and change nothing. `nereusd release` is recognized here; the daemon
// entry point coordinates its asynchronous reply and handover.
//
// The replies are what the operator reads: plain words, "Core" for the
// computer. They may carry the pairing code; nothing here logs it.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-24: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
// =================================================================

#include "core/daemon/StationControlSocket.h"
#include "core/daemon/StationStatusPage.h"

#include <QString>
#include <QStringList>

#include <functional>

namespace NereusSDR {

class StationServer;

class StationControlCommands {
public:
    struct Sources {
        /// The Core's pairing and devices; null while remote access is off.
        std::function<StationServer*()> server;
        std::function<StationRadioStatus()> radio;
        /// The Core's label as shown.
        std::function<QString()> label;
        /// The status page's address for the operator; "" when off.
        std::function<QString()> statusPageAddress;
    };

    explicit StationControlCommands(Sources sources);

    /// Runs one command: `args` as the console typed them, the command
    /// words first, then --unclaimed and --yes when given.
    StationControlReply execute(const QStringList& args) const;

    /// The first word of every command, so nereusd can tell a command from
    /// running the Core.
    static bool isCommand(const QString& word);
    /// The command list, as printed for an unknown command.
    static QString usage();

private:
    StationControlReply status() const;
    StationControlReply pairingShow(const QString& lead = QString()) const;
    StationControlReply pairingOpen() const;
    StationControlReply pairingClose() const;
    StationControlReply devices() const;
    StationControlReply revoke(const QString& id) const;
    StationControlReply retireToken() const;
    StationControlReply reset(bool unclaimed, bool yes) const;
    StationServer* server() const;

    Sources m_sources;
};

} // namespace NereusSDR
