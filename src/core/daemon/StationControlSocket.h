#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// src/core/daemon/StationControlSocket.h  (NereusSDR)
// =================================================================
//
// The Core's console channel (iPhone app plan Task 17, R-IOS-08; spec
// section 9, console command names). The running nereusd listens on a
// local socket named `nereusd-control` in its state directory, and the
// console subcommands (`nereusd status`, `nereusd pairing show`, ...) are
// a second nereusd process that sends one request over it and prints the
// answer (StationControlCommands decides what each one does).
//
// Who may connect: the account the Core runs as, and root. The socket is
// made with QLocalServer::UserAccessOption (mode 0700 before it takes its
// name), and the state directory is the account's own. On a packaged Core
// that account is systemd's DynamicUser and the directory is
// /var/lib/nereusd (mode 0700), so the commands run with sudo there.
//
// Where it is: socketPathFor(), the same on both sides. nereusd.conf's
// `state_directory` when set (the shipped sample names /var/lib/nereusd,
// so `sudo nereusd status` finds it through the default --config), or else
// the profile's own directory (AppSettings::resolveConfigDir, from
// --profile), which is under the Core's $HOME.
//
// A console command also looks where a packaged Core keeps that profile
// directory (candidatePathsFor()): a packaged unit pins HOME to
// /var/lib/nereusd, and under sudo the command's own $HOME is root's, so
// a Core whose /etc/nereusd.conf has no state_directory (an older sample,
// or a kit's own file) is found there with no extra options. The
// command's own place is tried first, so a Core started by hand is found
// exactly as before. Nothing about who may connect changes: root reaches
// the packaged directory and socket because it is root.
//
// One request per connection: a line of JSON, {"args": [...]}, at most
// kMaxRequestBytes; one reply, {"ok": bool, "text": "..."}, and the Core
// closes. The reply may carry the pairing code (the console is where the
// code belongs, pairing design section 4.3); nothing here logs a request
// or a reply.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-24: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-26: a console command also looks in a packaged Core's HOME,
//               and the no-answer text says what works (R-IOS-08,
//               R-R3-26). J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
// =================================================================

#include <QObject>
#include <QString>
#include <QStringList>

#include <functional>

class QLocalServer;
class QLocalSocket;

namespace NereusSDR {

struct DaemonConfig;

/// What a console command came to: printed as is, and the exit status.
struct StationControlReply {
    bool ok = false;
    QString text;
};

class StationControlSocket : public QObject {
    Q_OBJECT

public:
    static constexpr const char* kSocketName = "nereusd-control";
    static constexpr int kMaxRequestBytes = 4096;
    static constexpr int kMaxReplyBytes = 256 * 1024;
    /// A request not whole by then is closed.
    static constexpr int kRequestTimeoutMs = 5000;
    /// How long a console command waits for the Core.
    static constexpr int kClientTimeoutMs = 15000;
    /// Past this many bytes a failed listen() says the path may be too
    /// long: sun_path holds 104 bytes on macOS and 108 on Linux, and Qt
    /// binds in a private directory beside the name first.
    static constexpr int kLongPathBytes = 80;

    using Handler = std::function<StationControlReply(const QStringList& args)>;
    // Return true to retain the parsed request for a later reply. The
    // accepted socket remains parented to StationControlSocket even when
    // its listener is closed; the owner must finish it with sendReply().
    using AsyncHandler = std::function<bool(const QStringList& args, QLocalSocket* socket)>;

    explicit StationControlSocket(Handler handler, QObject* parent = nullptr,
                                  AsyncHandler asyncHandler = {});
    ~StationControlSocket() override;

    /// Listens at `path`, owner-only. A stale socket file left by a Core
    /// that stopped is replaced; one that another running Core answers on
    /// is not (false, lastError() says so). Creates the directory (0700)
    /// when missing.
    bool listen(const QString& path);
    void close();
    bool isListening() const;
    QString path() const { return m_path; }
    QString lastError() const { return m_lastError; }

    /// The directory the socket lives in: `state_directory`, or the
    /// profile's own directory.
    static QString directoryFor(const DaemonConfig& config, const QString& profile);
    /// directoryFor() plus kSocketName.
    static QString socketPathFor(const DaemonConfig& config, const QString& profile);

    /// Where a packaged Core's HOME is: the unit's Environment=HOME
    /// (/var/lib/nereusd) and the directory systemd's DynamicUser keeps it
    /// in (/var/lib/private/nereusd). Linux only; empty elsewhere.
    static QStringList packagedHomes();
    /// Every place a console command looks for the socket, in order:
    /// socketPathFor() first, then, when `state_directory` is not set, the
    /// profile's directory under each of `homes` (packagedHomes() by
    /// default). No path appears twice.
    static QStringList candidatePathsFor(const DaemonConfig& config, const QString& profile);
    static QStringList candidatePathsFor(const DaemonConfig& config, const QString& profile,
                                         const QStringList& homes);
    /// Tests only: replaces packagedHomes() in this process.
    static void setPackagedHomesForTest(const QStringList& homes);

    /// A console command: sends `args` to the Core at `path` and waits for
    /// its reply. Blocking; needs no event loop. When no Core answers, the
    /// reply is not ok and says in plain words what to try.
    static StationControlReply request(const QString& path, const QStringList& args,
                                       int timeoutMs = kClientTimeoutMs);
    /// The same, for the first of `paths` that exists (the first path when
    /// none does). When no Core answers, the reply names every path tried.
    static StationControlReply request(const QStringList& paths, const QStringList& args,
                                       int timeoutMs = kClientTimeoutMs);
    static bool sendReply(QLocalSocket* socket, const StationControlReply& reply);

private:
    void onNewConnection();
    void serve(QLocalSocket* socket);

    Handler m_handler;
    AsyncHandler m_asyncHandler;
    QLocalServer* m_server = nullptr;
    QString m_path;
    QString m_lastError;
};

} // namespace NereusSDR
