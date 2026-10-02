#pragma once
// no-port-check: NereusSDR-original. R3 Core session presentation and actions.
#include "core/ConnectionState.h"
#include "core/session/RemoteStationOptions.h"
#include <QFrame>
#include <QObject>
#include <QPointer>
#include <QDialog>
#include <QUrl>

#include <functional>
#include <optional>

class QLabel;
class QPushButton;
class QTimer;

namespace NereusSDR {
class RadioModel;
class StationClient;
class RemoteMediaController;

/// The operator-local remote access server preference, shared by pairing
/// and the later Core connection.
QList<QUrl> configuredRemoteAccessServers();

// R-R3-38: why a remote window stopped for good, when it did. Anything
// that will fix itself (a dropped link, an end the Core marks retryable)
// is None: the window retries as before and shows no stop message.
enum class CoreStopNotice {
    None,
    TakenOver,       // another app connected to the Core and took over
    UpdateThisApp,   // this app's link version is older than the Core's
    UpdateCore,      // the Core's link version is older than this app's
    UpdateOlderSide, // too far apart, and the Core did not say which is older
    Refused,         // any other end the Core marked not retryable
    // iPhone app Task 18 (R-IOS-08), chosen by the end's code:
    DeviceRemoved,   // the Core removed this computer, or no longer has it paired
    PairingRequired, // the Core now signs in paired devices only
    IdentityChanged, // the Core at this address is not the one this computer paired with
};

// Uses session state, never radio connectivity, to decide whether an
// operator can connect or cancel. The same controller backs all surfaces.
class RemoteConnectionController final : public QObject {
    Q_OBJECT
public:
    RemoteConnectionController(StationClient* client, RadioModel* model,
                               RemoteStationOptions options, QObject* parent = nullptr);
    QString endpointText() const;
    QString statusText() const;
    QString radioText() const;
    QString detailText() const;
    ConnectionState state() const;
    bool canConnect() const;
    bool canDisconnect() const;
    // R-R3-38: the stop message, when the window stopped for good. The
    // title is short (it is also the title bar's status); the text says
    // what happened and what the buttons do. Both empty for None.
    CoreStopNotice stopNotice() const;
    QString stopTitle() const;
    QString stopText() const;
    // True when the stop message offers Take it back (a takeover only).
    bool offersTakeBack() const;
    // True when updating this app is the fix (Check for updates is shown
    // only where the app has an update check; see CoreStopBanner).
    bool updateThisAppHelps() const;
    /// iPhone app plan Task 27 fix wave: where the saved Core was last
    /// reached, read at each connectToStation() so a reconnect in the same
    /// window tries the store's current list, not the one this window was
    /// made with. Returns nullopt when it has none to say (the Core is not
    /// a saved one); the options' own list is used then.
    using CurrentOptionsSource = std::function<std::optional<RemoteStationOptions>()>;
    void setCurrentOptionsSource(CurrentOptionsSource source);
public slots:
    void connectToStation();
    void disconnectFromStation();
    // R-R3-38: Take it back. Today this connects again, which takes the
    // Core back at once. The seam for the iPhone plan's Part G, which asks
    // the other device first: it replaces this body, and every caller
    // (the stop message's button) stays as it is.
    void takeBack();
    void recoverMediaSession(quint32 expectedEpoch, const QString& reason);
signals:
    void changed();
    // R-R3-16 / R-R3-38: the operator disconnected (or cancelled a pending
    // retry) through disconnectFromStation(). Every operator Disconnect
    // surface calls that slot, so this is the one signal the window uses
    // to open Connections. Link loss, a Core that reports its radio
    // offline and every disconnect the app starts itself (a Core refusal,
    // preemption, shutdown) never emit it.
    void operatorDisconnected();
private:
    std::optional<RemoteStationOptions> currentOptions() const;
    QPointer<StationClient> m_client;
    QPointer<RadioModel> m_model;
    RemoteStationOptions m_options;
    bool m_operatorDisconnected = false;
    quint32 m_pendingMediaRecoveryEpoch = 0;
    int m_retryAttempt = 0;
    int m_retryDelayMs = 0;
    CurrentOptionsSource m_currentOptionsSource;
};

// A small modeless view of the configured Core. Full station selection and
// pairing remain a separate surface; this view always describes the live client.
// With a media controller, it also shows a "Remote audio" section (current
// status, audio quality, codec, output and health), the Opus or Lossless
// choice and a Retry button; without one the panel is unchanged.
class RemoteConnectionPanel final : public QDialog {
    Q_OBJECT
public:
    explicit RemoteConnectionPanel(RemoteConnectionController* controller,
                                   QWidget* parent = nullptr,
                                   RemoteMediaController* media = nullptr);
protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
private:
    // Height follows the wrapped text at the current width.
    void fitHeightToContent();
    // Polls the audio health once a second, only while the panel is shown.
    QTimer* m_audioTimer = nullptr;
    std::function<void()> m_refreshAudio;
};

// R-R3-38: the stop message over a remote window's content. It keeps the
// window's layout (a child of the window placed over the content, not a
// modal dialog, so the menus keep working) and is shown only while the
// controller has a stop notice. Buttons: Take it back (a takeover), Choose
// another Core (where the window has Connections to open) and Check for
// updates (where this app has an update check and updating it helps).
// Nothing here retries by itself.
class CoreStopBanner final : public QFrame {
    Q_OBJECT
public:
    explicit CoreStopBanner(RemoteConnectionController* controller,
                            QWidget* parent = nullptr);
    // Whether the window can open Connections (a window the connection
    // picker manages); without it Choose another Core is not shown.
    void setChooseAnotherCoreAvailable(bool available);
    // Whether this app has an update check to run.
    void setCheckForUpdatesAvailable(bool available);
    // Shows or hides the message to match the controller.
    void refresh();
signals:
    void chooseAnotherCoreRequested();
    void checkForUpdatesRequested();
    // Emitted after refresh() changes what is shown, so the window can
    // place the message again.
    void contentChanged();
private:
    QPointer<RemoteConnectionController> m_controller;
    QLabel* m_title = nullptr;
    QLabel* m_text = nullptr;
    QPushButton* m_takeBack = nullptr;
    QPushButton* m_chooseCore = nullptr;
    QPushButton* m_checkUpdates = nullptr;
    bool m_chooseAvailable = false;
    bool m_updatesAvailable = false;
};
} // namespace NereusSDR
