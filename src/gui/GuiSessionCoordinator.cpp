// no-port-check: NereusSDR-original. R-R3-38 complete station-session ownership.
// 2026-10-04 - Include the setup signal type for Qt 6.4; JJ Boyd, OpenAI Codex.
#include "gui/GuiSessionCoordinator.h"

#include "core/AppSettings.h"
#include "core/settings/SettingsProxy.h"
#include "core/session/StationServer.h"
#include "core/station/StationRadios.h"
#include "gui/GuiDesktopStationRuntime.h"
#include "gui/MainWindow.h"
#include "gui/SetupDialog.h"
#include "models/RadioModel.h"

#include <QApplication>
#include <QCloseEvent>
#include <QScopedValueRollback>
#include <utility>
#include <QScopeGuard>
#include <QTimer>

namespace NereusSDR {

GuiSessionCoordinator::GuiSessionCoordinator(QObject* parent) : QObject(parent)
{
    m_hostedDiscoveryRetry.setSingleShot(true);
    // Match the discovery monitor's five-second scan interval.
    m_hostedDiscoveryRetry.setInterval(5000);
    connect(&m_hostedDiscoveryRetry, &QTimer::timeout, this, [this] {
        if (m_hostedStartupPending && !m_quitPrepared && m_window && m_desktopRuntime
            && m_desktopRuntime->controller()->enabled()) {
            m_window->radioModel()->discovery()->startDiscovery();
        }
    });
    // Register before MainWindow's aboutToQuit handler disconnects its model.
    connect(qApp, &QCoreApplication::aboutToQuit, this, [this] {
        QString error;
        if (m_desktopConfigured && !prepareApplicationQuit(&error)) {
            emit stationOperationFailed(error);
        }
    });
}

GuiSessionCoordinator::~GuiSessionCoordinator()
{
    shutdown();
}

bool GuiSessionCoordinator::configureDesktopStation(const QString& profile, bool profileOwned,
                                                     StationServiceOptions options)
{
    if (m_window || m_replacing) { return false; }
    m_profile = profile;
    m_profileOwned = profileOwned;
    m_serviceOptions = std::move(options);
    m_desktopConfigured = true;
    qApp->installEventFilter(this);
    return true;
}

bool GuiSessionCoordinator::prepareApplicationQuit(QString* error)
{
    if (error) { error->clear(); }
    if (m_quitPrepared) { return true; }
    if (m_preparingQuit || m_replacing
        || (m_window && m_window->radioModel()->localConnectionSetupActive())) {
        if (error) { *error = tr("The radio connection is still finishing. Try closing the window again when it has finished."); }
        return false;
    }
    const QScopedValueRollback<bool> preparing(m_preparingQuit, true);
    m_backgroundServiceOptions.reset();
    if (m_desktopRuntime) {
        if (!m_desktopRuntime->prepareForRetirement(true, error)) { return false; }
        if (m_desktopRuntime->backgroundStartWanted()) {
            m_backgroundServiceOptions = m_desktopRuntime->backgroundServiceOptions();
        }
    }
    m_quitPrepared = true;
    return true;
}

bool GuiSessionCoordinator::eventFilter(QObject* watched, QEvent* event)
{
    const bool closingWindow = watched == m_window.get() && event->type() == QEvent::Close;
    const bool quitting = watched == qApp && event->type() == QEvent::Quit;
    if (m_desktopConfigured && !m_replacing && (closingWindow || quitting)) {
        QString error;
        if (!prepareApplicationQuit(&error)) {
            event->ignore();
            emit stationOperationFailed(error);
            return true;
        }
    }
    return QObject::eventFilter(watched, event);
}

bool GuiSessionCoordinator::canReplace(const StationStartupSelection& selection, QString* error) const
{
    if (error) { error->clear(); }
    const auto fail = [error](const QString& message) {
        if (error) { *error = message; }
        return false;
    };
    if (m_replacing) {
        return fail(tr("A Core switch is already in progress."));
    }
    if (m_preparingQuit || m_quitPrepared) {
        return fail(tr("This window is closing."));
    }
    if (selection.connection.isRemote()
        && !selection.connection.isValidRemoteTarget()) {
        return fail(tr("The selected Core needs a valid address or paired remote access route."));
    }
    if (m_window) {
        RadioModel* model = m_window->radioModel();
        if (model->localConnectionSetupActive()) {
            return fail(tr("Local radio initialization is still finishing. Disconnect can cancel the connection; switch when initialization has finished."));
        }
        // Remote transmit state is mirrored in TransmitModel; its local
        // MoxController intentionally never keys this computer's hardware.
        if (model->stationOnAirRefusal(nullptr)) {
            return fail(tr("End transmit or Tune before switching Cores."));
        }
    }

    return true;
}

bool GuiSessionCoordinator::replace(const StationStartupSelection& selection,
                                    bool startConnection, QString* error)
{
    if (!canReplace(selection, error)) { return false; }
    const QPointer<GuiSessionCoordinator> self(this);
    const bool previousReplacing = std::exchange(m_replacing, true);
    const auto replacing = qScopeGuard([self, previousReplacing] {
        if (self) { self->m_replacing = previousReplacing; }
    });
    if (m_desktopRuntime && !m_desktopRuntime->prepareForRetirement(false, error)) {
        return false;
    }
    if (!self) { return false; }
    const bool quitOnClose = QApplication::quitOnLastWindowClosed();
    QApplication::setQuitOnLastWindowClosed(false);
    const auto restoreQuit = qScopeGuard([quitOnClose] {
        QApplication::setQuitOnLastWindowClosed(quitOnClose);
    });

    ++m_generation;
    retireWindow();
    if (!self) { return false; }
    m_selection = selection;
    if (selection.connection.isRemote()) {
        m_proxy = std::make_unique<SettingsProxy>();
        // This must precede every model constructor: an unready proxy keeps
        // local defaults from being written into another Core's settings.
        AppSettings::instance().setRemoteBackend(m_proxy.get());
    }
    m_window = std::make_unique<MainWindow>(selection.connection, nullptr,
                                           MainWindow::ConnectionStartup::Deferred);
    m_window->setConnectionPickerManaged(true);
    m_window->installEventFilter(this);
    const QPointer<MainWindow> installedWindow(m_window.get());
    if (m_desktopConfigured && !selection.connection.isRemote()) { installDesktopStation(); }
    if (!self || !installedWindow || m_window.get() != installedWindow) { return false; }
    const quint64 generation = m_generation;
    connect(m_window.get(), &MainWindow::connectionsRequested, this, [this, generation] {
        // Queued events survive disconnect. An old window must not open a
        // picker over the replacement station or trigger a second switch.
        if (generation == m_generation && m_window) { emit connectionsRequested(); }
    }, Qt::QueuedConnection);
    m_window->show();
    if (!self || !installedWindow || m_window.get() != installedWindow) { return false; }
    emit windowChanged(m_window.get());
    if (!self || !installedWindow || m_window.get() != installedWindow) { return false; }
    if (startConnection) { m_window->startInitialConnection(); }
    return true;
}

void GuiSessionCoordinator::retireWindow()
{
    m_hostedStartupPending = false;
    m_hostedRadioRecovery = false;
    m_hostedRadioAttempted = false;
    m_hostedDiscoveryRetry.stop();
    if (m_window && m_desktopRuntime) {
        m_window->setDesktopStationController(nullptr);
    }
    m_desktopRuntime.reset();
    m_stationRadios.reset();
    if (m_window) {
        m_window->retireForSessionSwitch();
        m_window.reset();
    }
    // AppSettings' backend is non-owning. It stays available throughout the
    // outgoing window/model destruction and is detached before proxy release.
    AppSettings::instance().setRemoteBackend(nullptr);
    m_proxy.reset();
}

void GuiSessionCoordinator::installDesktopStation()
{
    RadioModel* model = m_window->radioModel();
    const quint64 generation = m_generation;
    m_stationRadios = std::make_unique<StationRadios>(AppSettings::instance());
    m_stationRadios->onSelect = [this, generation](const QString& mac) {
        if (generation == m_generation) { queueHostedRadioChange(mac); }
    };
    m_stationRadios->onRescan = [this, generation] {
        if (generation == m_generation && m_window && !m_quitPrepared) {
            m_window->radioModel()->discovery()->startDiscovery();
        }
    };
    RuntimeStationBindings bindings;
    bindings.stationRadios = m_stationRadios.get();
    bindings.selectedRadioMac = [this, generation] {
        return generation == m_generation && m_stationRadios
            ? m_stationRadios->target() : QString();
    };
    m_desktopRuntime = std::make_unique<GuiDesktopStationRuntime>(model,
        &AppSettings::instance(), m_profile, m_profileOwned, m_serviceOptions, bindings);
    const QString saved = m_stationRadios->savedChoice();
    m_stationRadios->setTarget(saved.isEmpty() ? m_desktopRuntime->config().radioMac : saved);
    // The desktop's station peripherals follow the same configured network
    // as the background Core, before the initial radio connection starts.
    model->setStationBind(m_desktopRuntime->config().stationBind);
    m_window->setDesktopStationController(m_desktopRuntime->controller());
    connect(m_window.get(), &MainWindow::setupDialogCreated,
            m_desktopRuntime.get(), &GuiDesktopStationRuntime::bindSetupDialog);
    connect(m_desktopRuntime.get(), &GuiDesktopStationRuntime::operationFailed,
            this, &GuiSessionCoordinator::stationOperationFailed);
    connect(m_stationRadios.get(), &StationRadios::entriesChanged, model, [this, model] {
        model->setStationRadioWaiting(m_stationRadios->waitingReason());
    });
    connect(m_window.get(), &MainWindow::hostedInitialConnectionRequested, this,
            [this, generation] {
        if (generation != m_generation || !m_window || m_quitPrepared) { return; }
        m_hostedRadioRecovery = true;
        m_hostedStartupPending = true;
        m_window->radioModel()->discovery()->startDiscovery();
    });
    connect(model->discovery(), &RadioDiscovery::discoveryFinished, this,
            [this, generation] { finishHostedDiscovery(generation); }, Qt::QueuedConnection);
    connect(model, &RadioModel::radioDisconnectRequested, this, [this] {
        if (m_retiringHostedRadio) { return; }
        m_hostedRadioRecovery = false;
        m_hostedStartupPending = false;
        m_hostedDiscoveryRetry.stop();
    });
    connect(model, &RadioModel::connectionStateChanged, this,
            [this, generation](ConnectionState state) {
        if (generation != m_generation || !m_window) { return; }
        RadioModel* current = m_window->radioModel();
        if (state != current->connectionState()) { return; }
        if (state == ConnectionState::LinkLost
            || (state == ConnectionState::Disconnected && current->connection())) {
            retryHostedRadio(generation);
        }
    }, Qt::QueuedConnection);
    const auto refresh = [this, generation] {
        if (generation == m_generation) { refreshStationRadios(); }
    };
    connect(model, &RadioModel::connectionStateChanged, this, refresh);
    connect(model, &RadioModel::infoChanged, this, refresh);
    connect(model->discovery(), &RadioDiscovery::radioDiscovered, this, refresh);
    connect(model->discovery(), &RadioDiscovery::radioUpdated, this, refresh);
    connect(model->discovery(), &RadioDiscovery::radioLost, this, refresh);
    refreshStationRadios();
    const QPointer<GuiSessionCoordinator> self(this);
    const QPointer<RadioModel> localModel(model);
    m_desktopRuntime->restore();
    if (!self || !localModel || generation != m_generation || !m_window
        || m_window->radioModel() != localModel) { return; }
    // Initial host ownership adoption is part of the startup seed. Every
    // subsequent edit must be accounted for before handing this model away.
    model->beginStationHandoverEditTracking();
}

void GuiSessionCoordinator::finishHostedDiscovery(quint64 generation)
{
    if (generation != m_generation || !m_hostedStartupPending || !m_window
        || !m_stationRadios || !m_desktopRuntime || m_quitPrepared
        || !m_desktopRuntime->controller()->enabled()) { return; }
    RadioModel* model = m_window->radioModel();
    if (model->isConnected() || model->localConnectionSetupActive()
        || model->connectionState() == ConnectionState::Probing
        || model->connectionState() == ConnectionState::Connecting) {
        m_hostedStartupPending = false;
        m_hostedDiscoveryRetry.stop();
        return;
    }
    const QList<RadioInfo> found = model->discovery()->discoveredRadios();
    m_stationRadios->setVisible(found);
    const StationRadios::Choice choice = StationRadios::choose(found,
        m_stationRadios->target(), m_stationRadios->savedChoice(),
        m_desktopRuntime->config().radioMac);
    if (choice.pick != StationRadios::Pick::Radio) {
        m_stationRadios->setWaiting(choice.reason);
        m_hostedDiscoveryRetry.start();
        return;
    }
    if (model->stationOnAirRefusal(nullptr)) { return; }
    RadioInfo radio = choice.radio;
    const HPSDRModel override = m_stationRadios->overrideFor(radio.macAddress);
    if (override != HPSDRModel::FIRST) { radio.modelOverride = override; }
    m_hostedStartupPending = false;
    m_hostedDiscoveryRetry.stop();
    m_stationRadios->setTarget(radio.macAddress);
    m_stationRadios->setCurrent(radio);
    connectHostedRadio(radio);
}

void GuiSessionCoordinator::connectHostedRadio(const RadioInfo& radio)
{
    RadioModel* model = m_window->radioModel();
    const bool preserve = m_hostedRadioAttempted
        && model->currentRadioInfo().macAddress.compare(radio.macAddress, Qt::CaseInsensitive) == 0;
    m_hostedRadioAttempted = true;
    if (preserve) { model->connectToRadioPreservingSlices(radio); }
    else { model->connectToRadio(radio); }
    const quint64 generation = m_generation;
    if (RadioConnection* connection = model->connection()) {
        const QPointer<RadioConnection> attempted(connection);
        connect(connection, &RadioConnection::connectFailed, this,
                [this, generation, attempted](ConnectFailure, const QString&) {
            if (generation == m_generation && m_window && attempted
                && m_window->radioModel()->connection() == attempted) {
                retryHostedRadio(generation);
            }
        }, Qt::QueuedConnection);
    } else {
        retryHostedRadio(generation);
    }
}

void GuiSessionCoordinator::retryHostedRadio(quint64 generation)
{
    if (generation != m_generation || !m_hostedRadioRecovery || m_retiringHostedRadio
        || !m_window || !m_desktopRuntime || m_quitPrepared
        || !m_desktopRuntime->controller()->enabled()) { return; }
    RadioModel* model = m_window->radioModel();
    if (model->localConnectionSetupActive()) {
        QTimer::singleShot(100, this, [this, generation] { retryHostedRadio(generation); });
        return;
    }
    // Retire failed I/O/DSP while retaining receiver objects and edits. This
    // internal disconnect is distinct from the operator's Disconnect intent.
    const QScopedValueRollback<bool> retiring(m_retiringHostedRadio, true);
    const QString attempted = model->currentRadioInfo().macAddress;
    if (!attempted.isEmpty()) { m_stationRadios->setTarget(attempted); }
    // TX safety fix round 2 (2026-09-30): the recovery retire keeps a
    // lost-link key lock until the rebuilt link is Connected.
    model->retireConnectionForRecovery();
    m_stationRadios->clearCurrent();
    m_stationRadios->setSwitching(false);
    m_stationRadios->setWaiting(tr("The Core is reconnecting to its radio."));
    m_hostedStartupPending = true;
    m_hostedDiscoveryRetry.start();
}

void GuiSessionCoordinator::refreshStationRadios()
{
    if (!m_window || !m_stationRadios) { return; }
    RadioModel* model = m_window->radioModel();
    m_stationRadios->setVisible(model->discovery()->discoveredRadios());
    if (model->isConnected()) {
        const RadioInfo radio = model->currentRadioInfo();
        if (!radio.macAddress.isEmpty()) {
            m_stationRadios->setTarget(radio.macAddress);
            m_stationRadios->setCurrent(radio);
            m_stationRadios->saveChoice(radio.macAddress);
            m_stationRadios->confirmChoice(radio.macAddress);
            m_stationRadios->setSwitching(false);
        }
    } else {
        m_stationRadios->clearCurrent();
        if (model->connectionState() == ConnectionState::Disconnected) {
            m_stationRadios->setSwitching(false);
        }
    }
}

void GuiSessionCoordinator::queueHostedRadioChange(const QString& mac)
{
    if (!m_desktopRuntime || !m_desktopRuntime->controller()->server()) { return; }
    m_window->radioModel()->setStationRadioChangeUnderway(true);
    m_desktopRuntime->controller()->server()->holdRadioChangeAnswers();
    const quint64 generation = m_generation;
    QTimer::singleShot(0, this, [this, mac, generation] {
        finishHostedRadioChange(mac, generation);
    });
}

void GuiSessionCoordinator::finishHostedRadioChange(const QString& mac, quint64 generation)
{
    if (generation != m_generation || !m_window || !m_stationRadios || !m_desktopRuntime) { return; }
    RadioModel* model = m_window->radioModel();
    if (model->localConnectionSetupActive()) {
        QTimer::singleShot(100, this, [this, mac, generation] {
            finishHostedRadioChange(mac, generation);
        });
        return;
    }
    const auto requested = m_stationRadios->radioFor(mac);
    QString error;
    if (!requested) { error = StationRadios::unknownRadioReason(); }
    else if (!canReplace({}, &error)) { /* retain the current window */ }
    else if (!model->saveForStationHandover(&error)) { /* retain unsaved state */ }
    if (!error.isEmpty()) {
        m_stationRadios->dropPendingChoice();
        m_stationRadios->setSwitching(false);
        model->setStationRadioChangeUnderway(false);
        if (auto* server = m_desktopRuntime->controller()->server()) {
            server->finishRadioChange(false, error);
        }
        emit stationOperationFailed(error);
        return;
    }
    RadioInfo radio = *requested;
    const HPSDRModel override = m_stationRadios->overrideFor(mac);
    if (override != HPSDRModel::FIRST) { radio.modelOverride = override; }
    if (auto* server = m_desktopRuntime->controller()->server()) {
        server->finishRadioChange(true, {});
        server->endSessionsForRadioChange(tr("The Core is switching to %1. This app reconnects by itself.")
                                              .arg(radio.displayName()));
    }
    if (!replace({}, false, &error)) {
        if (m_stationRadios) { m_stationRadios->dropPendingChoice(); m_stationRadios->setSwitching(false); }
        if (m_window) { m_window->radioModel()->setStationRadioChangeUnderway(false); }
        emit stationOperationFailed(error);
        return;
    }
    m_stationRadios->setTarget(mac);
    m_stationRadios->setSwitching(true);
    m_hostedRadioRecovery = true;
    connectHostedRadio(radio);
}

void GuiSessionCoordinator::shutdown()
{
    if (m_replacing || (!m_window && !m_proxy)) { return; }
    const QScopedValueRollback<bool> replacing(m_replacing, true);
    ++m_generation;
    const bool quitOnClose = QApplication::quitOnLastWindowClosed();
    QApplication::setQuitOnLastWindowClosed(false);
    retireWindow();
    QApplication::setQuitOnLastWindowClosed(quitOnClose);
    emit windowChanged(nullptr);
}

} // namespace NereusSDR
