// no-port-check: NereusSDR-original.
// =================================================================
// tests/tools/nereus_pairing_peer.cpp  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 14 (R-IOS-08): the Core's side of one pairing, over
// standard input and output, for the cross-implementation tests (the
// iPhone app's pairing interop test in its Task 15, and
// tst_station_pairing here).
//
// It runs a real StationServer, with its identity, certificate and paired
// devices in a scratch directory made for this run, and hands it one
// connection whose messages are lines: each line on standard input is one
// message from the device, each line on standard output one message from
// the Core. Exactly what the Core would send over its WebSocket, one
// compact JSON object per line.
//
// Two lines of its own frame the run, both on standard output:
//
//   {"type":"peer.ready","code":"<the pairing code>","certSha256":"<b64url>"}
//       first, before the Core's hello. `code` is the window's current code
//       ("" when the window is closed); `certSha256` the SHA-256 of the
//       Core's TLS certificate, which the identity's certBinding signs.
//       The code is this scratch Core's own, made for this run; it reaches
//       only whoever reads this process's standard output.
//   {"type":"peer.done","paired":true|false,"devices":<count>}
//       last, when the connection has ended; then the program exits 0.
//
// Options:
//   --lan-deny          pairing_lan_click = deny
//   --address <ip>      the address the connection reports as the device's
//                       (default 127.0.0.1, which is on a directly
//                       connected network, so one tap is allowed)
//   --claimed           pair a device made for this run first, then reopen
//                       the window (OpenReopened): the code pairs, one tap
//                       does not
//
// Nothing is logged but warnings, to standard error; the code never is.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-27: Optional startup phase timings for load-failure diagnosis.
//               J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-09-24: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
// =================================================================

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QMetaObject>
#include <QPointer>
#include <QStringList>
#include <QTemporaryDir>

#include <cstdio>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include "core/AppSettings.h"
#include "core/security/DeviceStore.h"
#include "core/security/PairingWindow.h"
#include "core/security/StationIdentity.h"
#include "core/session/SessionTransport.h"
#include "core/session/StationServer.h"
#include "models/RadioModel.h"

using namespace NereusSDR;

namespace {

void writeLine(const QByteArray& line)
{
    std::fwrite(line.constData(), 1, static_cast<size_t>(line.size()), stdout);
    std::fputc('\n', stdout);
    std::fflush(stdout);
}

// One connection whose messages are lines on standard input and output.
class StdioTransport : public SessionTransport {
    Q_OBJECT

public:
    explicit StdioTransport(const QString& address, QObject* parent = nullptr)
        : SessionTransport(parent)
        , m_address(address)
    {
    }

    void sendText(const QByteArray& wire) override
    {
        if (m_open && !wire.isEmpty()) {
            writeLine(wire);
        }
    }
    void ping() override
    {
        // Lines carry no ping; the peer's liveness is its standard input.
    }
    void closeLink(const QString&) override
    {
        if (!m_open) {
            return;
        }
        m_open = false;
        emit closed();
    }
    bool isOpen() const override { return m_open; }
    QString peerDescription() const override { return QStringLiteral("stdio"); }
    QString peerAddress() const override { return m_address; }

    // From the reader thread, queued onto this object's thread.
    void receiveLine(const QByteArray& line)
    {
        if (m_open && !line.trimmed().isEmpty()) {
            emit textReceived(line.trimmed());
        }
    }
    void endOfInput() { closeLink(QString()); }

private:
    QString m_address;
    bool m_open = true;
};

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    QElapsedTimer startup;
    startup.start();
    const auto markStartup = [&](const char* phase) {
        if (qEnvironmentVariableIsSet("NEREUS_PAIRING_STARTUP_TIMINGS")) {
            std::fprintf(stderr, "pairing-startup %s %lld ms\n", phase,
                         static_cast<long long>(startup.elapsed()));
            std::fflush(stderr);
        }
    };
    markStartup("main");
    // Warnings only: the Core's informational lines would be noise here.
    QLoggingCategory::setFilterRules(QStringLiteral("*.debug=false\n*.info=false"));

    const QStringList args = app.arguments();
    const bool lanDeny = args.contains(QStringLiteral("--lan-deny"));
    const bool claimed = args.contains(QStringLiteral("--claimed"));
    QString address = QStringLiteral("127.0.0.1");
    const int addressAt = static_cast<int>(args.indexOf(QStringLiteral("--address")));
    if (addressAt >= 0 && addressAt + 1 < args.size()) {
        address = args.at(addressAt + 1);
    }

    QTemporaryDir scratch;
    if (!scratch.isValid()) {
        std::fprintf(stderr, "nereus_pairing_peer: no scratch directory\n");
        return 2;
    }
    const QString security = scratch.filePath(QStringLiteral("core"));
    AppSettings settings(scratch.filePath(QStringLiteral("NereusSDR.settings")));
    markStartup("before-model");
    RadioModel model;
    markStartup("after-model");
    // The first-run banner goes to standard output on the run that makes the
    // identity key; make it first so that output carries only lines.
    StationIdentity::loadOrCreate(security);
    markStartup("after-identity");
    StationServer server(&model, settings, security);
    markStartup("after-server");
    server.setHeartbeatIntervalMs(0);
    server.setPairingLanClickAllowed(!lanDeny);

    int pairedBefore = 0;
    std::unique_ptr<QTemporaryDir> deviceKeyDir;
    if (claimed) {
        deviceKeyDir = std::make_unique<QTemporaryDir>();
        const StationIdentity key = StationIdentity::loadOrCreate(deviceKeyDir->path());
        PairedDevice device;
        device.id = key.fingerprint();
        device.publicKeySpki = key.publicKeySpki();
        device.name = QStringLiteral("Bench computer");
        device.kind = QStringLiteral("computer");
        if (!server.deviceStore()->add(device)) {
            std::fprintf(stderr, "nereus_pairing_peer: could not pair the first device\n");
            return 2;
        }
        server.pairingWindow()->reopen();
        pairedBefore = 1;
    }

    markStartup("before-ready");
    QString pin = server.certificateFingerprint();
    pin.remove(QLatin1Char(':'));
    writeLine(QJsonDocument(QJsonObject{
                                {QStringLiteral("type"), QStringLiteral("peer.ready")},
                                {QStringLiteral("code"), server.pairingWindow()->currentCode()},
                                {QStringLiteral("certSha256"),
                                 StationIdentity::toBase64Url(QByteArray::fromHex(pin.toLatin1()))},
                            })
                  .toJson(QJsonDocument::Compact));

    auto* transport = new StdioTransport(address);
    QObject::connect(&server, &StationServer::peerDisconnected, &app,
                     [&app, &server, pairedBefore](const QString&, const QString&) {
                         const int devices = static_cast<int>(server.deviceStore()->list().size());
                         writeLine(QJsonDocument(QJsonObject{
                                                     {QStringLiteral("type"),
                                                      QStringLiteral("peer.done")},
                                                     {QStringLiteral("paired"),
                                                      devices > pairedBefore},
                                                     {QStringLiteral("devices"), devices},
                                                 })
                                       .toJson(QJsonDocument::Compact));
                         QMetaObject::invokeMethod(&app, &QCoreApplication::quit,
                                                   Qt::QueuedConnection);
                     });

    // Standard input on its own thread, each line queued to the transport.
    QPointer<StdioTransport> target(transport);
    std::thread reader([target]() {
        std::string line;
        while (std::getline(std::cin, line)) {
            const QByteArray bytes = QByteArray::fromStdString(line);
            QMetaObject::invokeMethod(
                target, [target, bytes]() {
                    if (!target.isNull()) { target->receiveLine(bytes); }
                },
                Qt::QueuedConnection);
        }
        QMetaObject::invokeMethod(
            target, [target]() {
                if (!target.isNull()) { target->endOfInput(); }
            },
            Qt::QueuedConnection);
    });
    reader.detach();

    server.acceptTransport(transport);
    const int result = app.exec();
    std::fflush(stdout);
    // The scratch directories go now: the reader may still be blocked on
    // standard input, so the process ends without running destructors.
    server.close();
    if (deviceKeyDir) {
        deviceKeyDir->remove();
    }
    scratch.remove();
    std::_Exit(result);
}

#include "nereus_pairing_peer.moc"
