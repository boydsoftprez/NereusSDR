#pragma once
// =================================================================
// tests/fakes/RemoteWindowHarness.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. Test fixture that joins one real
// MainWindow to an in-process Core; no upstream logic is ported.
//
// R3 remote window harness (R-R3-16, R-R3-17, R-R3-21, R-R3-24).
//
// One real MainWindow in remote role, dialling a real StationServer that
// runs in the same process. The window uses its production dial path
// (RemoteConnectionController -> StationClient::connectToStation ->
// QWebSocket), so a test starts every connection through a real QAction
// or widget, never by calling StationClient directly. The Core accepts
// each socket through CoreSessionTransport, a WebSocketTransport that the
// test can hold at the snapshot boundary:
//
//   - holdNextSnapshot() lets the hello, authentication and capability
//     exchange through and holds everything after it (settings snapshot,
//     object creates, snapshot-complete marker) until releaseSnapshot().
//     That is the window between "radio connected" and "slices known"
//     where the startup extra-slice bug lived.
//   - dropLink() closes the Core's end of the live session, the way a
//     Core restart or a network drop looks from the window.
//   - The Core's capabilities carry Options::radioName: the bench Core
//     model has no connected radio to name, and a real Core always sends
//     one (the window's retained-name reopen path depends on it).
//   - reportRadioOffline() takes the Core's radio offline with the
//     session still up, so the window receives the same "connected"
//     delta a real Core sends when its radio drops.
//   - pushCapabilities() sends a capability descriptor on the live
//     session, the same message StationServer sends when its advertised
//     capabilities change after the snapshot.
//   - addSliceCommands() lists every add-slice command the Core received,
//     so "the window created no slice" is read on the Core's side of the
//     wire.
//   - Options::sliceAccess makes the window's bench link declare
//     sessionHolder and sliceAccess, as a device-key sign-in does, so the
//     window shares slices with the Core's other devices.
//   - holdSliceAccessUpdates() holds the Core's `access:<id>` objects (who
//     controls and who listens to each slice) while everything else goes
//     through, so a test can make a command's answer arrive before the
//     access change it caused.
//   - sliceAccessCommands() lists the slice.* access verbs the Core
//     received.
//
// Why a WebSocket and not tests/fakes/LoopbackTransport: MainWindow has no
// transport seam. It dials its configured URL, and the acceptance for this
// harness is that every connection starts from the window's own controls.
// A loopback ws:// listener is the lightest Core fixture the existing
// window tests (tst_gui_connection_controller) already use for that.
//
// Settings isolation: the process-wide AppSettings must already point at
// an isolated profile (useIsolatedProfile() below) before the harness is
// built. The harness installs its own SettingsProxy as the remote backend
// for the window's lifetime and removes it again before destroying it.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-23  J.J. Boyd / KG4VCF  R3 remote window harness plan, Task 2.
//                                    AI-assisted transformation via
//                                    Anthropic Claude Code.
//   2026-09-23  J.J. Boyd / KG4VCF  R3 remote window Setup plan, Task 3:
//                                    reportRadioOffline(). AI-assisted
//                                    transformation via Anthropic Claude
//                                    Code.
//   2026-09-29  J.J. Boyd / KG4VCF  Slice control plan Task 11 fix:
//                                    txSliceCommands().
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  Slice control plan Task 17: options
//                                    for a window that shares slices,
//                                    holding the Core's slice-access
//                                    updates, sliceAccessCommands().
//                                    AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QByteArray>
#include <QList>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QWebSocketServer>

#include <memory>

#include "core/AppSettings.h"
#include "core/session/SessionTransport.h"
#include "core/session/StationCapabilities.h"
#include "core/session/StationServer.h"
#include "core/settings/SettingsProxy.h"
#include "gui/MainWindow.h"
#include "models/RadioModel.h"

class QAction;
class QWebSocket;

namespace NereusSDR {
class ConnectionSegment;
class RemoteConnectionController;
class SpectrumWidget;
class StationBlock;
class StationClient;
} // namespace NereusSDR

namespace NereusSDR::Test {

// The Core's end of one session. Everything production sends goes through
// sendText(); while held, it queues instead and goes out in order on
// release.
class CoreSessionTransport final : public WebSocketTransport {
    Q_OBJECT

public:
    CoreSessionTransport(QWebSocket* socket, bool holdAfterCapabilities,
                         const QString& radioName, QObject* parent = nullptr);

    void sendText(const QByteArray& wire) override;

    /// Sends now, bypassing any hold.
    void sendDirect(const QByteArray& wire);

    /// Sends everything held, in order, and stops holding.
    void release();

    /// Holds every `access:<id>` object message until
    /// releaseSliceAccessUpdates(); everything else still goes out.
    void holdSliceAccessUpdates() { m_holdingAccess = true; }
    void releaseSliceAccessUpdates();
    int heldSliceAccessCount() const { return m_heldAccess.size(); }

    bool holding() const { return m_holding; }
    int heldCount() const { return m_held.size(); }
    bool capabilitiesSent() const { return m_capabilitiesSent; }

private:
    QByteArray named(const QByteArray& wire) const;

    bool m_holdAfterCapabilities = false;
    bool m_capabilitiesSent = false;
    bool m_holding = false;
    QList<QByteArray> m_held;
    bool m_holdingAccess = false;
    QList<QByteArray> m_heldAccess;
    QString m_radioName;
};

class RemoteWindowHarness final {
public:
    struct Options {
        /// Slices the Core already has, each on "pan-<n>".
        int stationSlices = 1;
        /// The GUI's saved pan layout (PanLayoutId), restored at startup.
        QString panLayout = QStringLiteral("1");
        /// Reconnect backoff unit for the window's StationClient.
        int backoffUnitMs = 50;
        /// The radio name the Core reports in its capabilities.
        QString radioName = QStringLiteral("Bench Saturn");
        /// The window shares slices: its bench link declares
        /// sessionHolder and sliceAccess, numbered by the Core as
        /// sessionHolderId.
        bool sliceAccess = false;
        QString sessionHolderId = QStringLiteral("token:1");
    };

    /// Points AppSettings at a profile of its own and empties it. Call
    /// once from initTestCase(), before any window or harness exists, and
    /// QVERIFY the result: false means AppSettings was already on another
    /// file, which is then left untouched.
    [[nodiscard]] static bool useIsolatedProfile(const QString& tag);
    /// Empties the isolated profile between cases. False, touching
    /// nothing, unless AppSettings is still on the verified profile.
    [[nodiscard]] static bool clearIsolatedProfile();
    /// Deletes the isolated profile's files. Call from cleanupTestCase().
    /// Same refusal as clearIsolatedProfile().
    [[nodiscard]] static bool removeIsolatedProfile();

    RemoteWindowHarness();
    explicit RemoteWindowHarness(const Options& options);
    ~RemoteWindowHarness();

    RemoteWindowHarness(const RemoteWindowHarness&) = delete;
    RemoteWindowHarness& operator=(const RemoteWindowHarness&) = delete;

    /// Starts the Core listening, then builds the window (connection start
    /// deferred, as GuiSessionCoordinator builds it) and shows it. False
    /// when the listener could not start.
    bool start();

    /// Starts the window's own startup connection, exactly as the
    /// application does after constructing a window.
    void startStartupConnection();

    // ---- The Core ----
    RadioModel& station() { return m_station; }
    StationServer& server() { return m_server; }
    /// Sockets the Core has accepted so far: one per dial that arrived.
    int acceptedConnections() const { return m_accepted; }
    /// The Core's end of the most recent session, or null.
    CoreSessionTransport* coreLink() const { return m_link; }

    /// The next session the Core accepts holds everything after its
    /// capability exchange until releaseSnapshot().
    void holdNextSnapshot() { m_holdNext = true; }
    void releaseSnapshot();
    bool snapshotHeld() const;

    /// Core closes the live session.
    void dropLink(const QString& reason = QStringLiteral("Core link dropped"));

    /// The Core's radio goes offline while the session stays up: the
    /// station model reports Disconnected, and the Core mirrors its
    /// RadioModel "connected" property to the window as it would for a
    /// real radio that dropped.
    void reportRadioOffline();

    /// Core sends this capability descriptor on the live session.
    void pushCapabilities(const StationCapabilities& capabilities);

    /// The Core's own settings store (what the Core holds, as opposed to
    /// what the window caches).
    AppSettings& stationSettings() { return m_stationSettings; }

    /// Every addSlice / addSliceOnPan command the Core received, as
    /// "<verb>:<pan>".
    QStringList addSliceCommands() const { return m_addSliceCommands; }
    /// The slice id of every tx.setTxSlice command the Core received.
    QList<int> txSliceCommands() const { return m_txSliceCommands; }
    /// Every slice.listen / slice.stopListening / slice.takeControl /
    /// slice.release command the Core received, as "<verb>:<sliceId>".
    QStringList sliceAccessCommands() const { return m_sliceAccessCommands; }

    // ---- The window ----
    MainWindow* window() const { return m_window.get(); }
    /// The window's settings proxy: the one path from this window's
    /// settings to the Core.
    SettingsProxy& proxy() { return m_proxy; }
    RadioModel* remoteModel() const;
    StationClient* client() const;
    RemoteConnectionController* controls() const;
    ConnectionSegment* titleSegment() const;
    StationBlock* stationBlock() const;
    SpectrumWidget* panSpectrum(const QString& panId) const;
    /// A menu action by its menu title and action text, as the operator
    /// sees them (for example "&Radio", "&Connect").
    QAction* menuAction(const QString& menuTitle, const QString& actionText) const;

private:
    void accept();
    void recordInbound(const QByteArray& wire);

    Options m_options;
    QTemporaryDir m_directory;
    AppSettings m_stationSettings;
    // The Core's model and server are built before the window's proxy is
    // installed, as src/main.cpp and nereusd order them.
    RadioModel m_station;
    StationServer m_server;
    QWebSocketServer m_listener;
    SettingsProxy m_proxy;
    std::unique_ptr<MainWindow> m_window;
    QPointer<CoreSessionTransport> m_link;
    int m_accepted = 0;
    bool m_holdNext = false;
    bool m_backendInstalled = false;
    QStringList m_addSliceCommands;
    QList<int> m_txSliceCommands;
    QStringList m_sliceAccessCommands;
};

} // namespace NereusSDR::Test
