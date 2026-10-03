// no-port-check: NereusSDR-original Core Settings host and page authority.
// 2026-10-02 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#include "gui/CoreSettingsHost.h"
#include "gui/GuiConnectionController.h"
#include "gui/SetupDialog.h"
#include "gui/setup/CoresSetupPage.h"
#include "core/security/ClientDeviceIdentity.h"
#include <QDataStream>
#include <algorithm>
namespace NereusSDR {
namespace {
quint64 nextAuthorityEpoch()
{
    // All host/page operations run on the GUI thread. Never coordinator/client epochs.
    static quint64 epoch = 0;
    return ++epoch;
}
CoreRenameController::Request helperRequest(const CoreRenameRequest& request)
{
    return {request.operationId, request.targetId, request.pairedIdentity,
            request.incarnation, request.epoch, request.name};
}
CoreRenameRequest pageRequest(const CoreRenameController::Request& request)
{
    return {request.operationId, request.targetId, request.pairedIdentity,
            request.incarnation, request.epoch, request.name};
}
}
CoreSettingsHost::CoreSettingsHost(CoreTargetStore& store, ContextSource context,
    CoreRenameController::SessionSource sessions, QObject* parent,
    CoreRenameController::TemporaryFactory factory, CoreRenameController::Starter starter,
    CoreRenameController::Limits limits)
    : QObject(parent), m_store(store), m_contextSource(std::move(context)),
      m_sessions(std::move(sessions)), m_factory(std::move(factory)),
      m_starter(std::move(starter)), m_limits(limits)
{}
CoreSettingsHost::~CoreSettingsHost()
{
    retireBindings();
    if (m_rename) { disconnect(m_rename.get(), nullptr, this, nullptr); }
    m_rename.reset(); // Before the application destroys its canonical store.
}
void CoreSettingsHost::setExistingDeviceIdentity(std::shared_ptr<const ClientDeviceIdentity> identity)
{
    // Absence in a local window does not discard the key loaded by an earlier remote window.
    if (!identity || !identity->isValid() || m_identity == identity) { return; }
    if (m_rename) {
        for (const PageState& state : m_pages) { if (state.page) { state.page->cancelOperations(); } }
        disconnect(m_rename.get(), nullptr, this, nullptr);
        m_rename.reset();
        m_routes.clear();
        m_active.reset();
    }
    m_identity = std::move(identity);
    m_rename = std::make_unique<CoreRenameController>(m_store,
        [this](const QByteArray& paired) { return sessionSnapshot(paired); }, m_identity,
        [this](const CoreRenameController::Request& request) { return requestCurrent(request); },
        nullptr, m_factory, m_starter, m_limits);
    connect(m_rename.get(), &CoreRenameController::temporaryAdmissionChanged, this, [this] {
        refresh();
        emit admissionChanged();
    }, Qt::QueuedConnection);
    connect(m_rename.get(), &CoreRenameController::finished, this,
        [this](const CoreRenameController::Request& request, CoreRenameController::Outcome outcome,
               const QString&, const QString& reason) {
            const CoreRenameRequest exact = pageRequest(request);
            const auto route = std::find_if(m_routes.begin(), m_routes.end(),
                [&exact](const Route& candidate) { return candidate.request == exact; });
            if (route == m_routes.end()) { return; }
            const QPointer<CoresSetupPage> origin = route->page;
            m_routes.erase(route);
            if (m_active && m_active->request == exact) { m_active.reset(); }
            if (origin) {
                origin->finishRename(exact, outcome == CoreRenameController::Outcome::Accepted
                    || outcome == CoreRenameController::Outcome::AcceptedLocalSaveFailed, reason);
            }
            refresh();
        });
    refresh();
}
CoreRenameController::Snapshot CoreSettingsHost::sessionSnapshot(const QByteArray& identity) const
{
    CoreRenameController::Snapshot snapshot = m_sessions ? m_sessions(identity) : CoreRenameController::Snapshot{};
    // The helper's current temporary client is excluded; older drains remain exclusion owners.
    if (m_rename && m_rename->ownsDrainingAdmission(identity)) { snapshot.sameCoreActive = true; }
    return snapshot;
}
bool CoreSettingsHost::ownsTemporaryAdmission(const QByteArray& identity) const
{
    return identity.size() == 32 && m_rename && m_rename->ownsTemporaryAdmission(identity);
}
QString CoreSettingsHost::admissionUnavailableReason(const QByteArray& identity) const
{
    return ownsTemporaryAdmission(identity)
        ? tr("A temporary rename session for this Core is still signing in or closing. Wait for it to finish before connecting.")
        : QString();
}
CoreSettingsContext CoreSettingsHost::currentContext()
{
    CoreSettingsContext context = m_contextSource ? m_contextSource() : CoreSettingsContext{};
    QByteArray sourceAuthority;
    QDataStream sourceStream(&sourceAuthority, QIODevice::WriteOnly);
    sourceStream << context.targetId << context.pairedIdentity << context.epoch << context.authenticated;
    if (sourceAuthority != m_contextSourceAuthority) {
        m_contextSourceAuthority = sourceAuthority;
        m_contextSourceIncarnation = m_store.targetIncarnation(context.targetId);
    }
    if (context.authenticated && (m_contextSourceIncarnation == 0
        || m_store.targetIncarnation(context.targetId) != m_contextSourceIncarnation)) {
        context.authenticated = false;
        context.coreName.clear();
        context.stationSettingsAvailable = false;
        context.stationSettingsReason = tr("The saved Core is no longer writable in this window.");
        context.controls = tr("Path unavailable");
        context.audioAndDisplay = tr("Path unavailable");
        context.listener = tr("Not reported");
        context.radio.clear();
    }
    QByteArray authority;
    QDataStream stream(&authority, QIODevice::WriteOnly);
    stream << context.targetId << context.pairedIdentity << context.epoch << context.authenticated;
    if (authority != m_currentAuthority) {
        m_currentAuthority = authority;
        m_currentEpoch = nextAuthorityEpoch();
    }
    context.epoch = m_currentEpoch;
    return context;
}
void CoreSettingsHost::bindDialog(SetupDialog* dialog)
{
    if (!dialog || m_retiring) { return; }
    for (const auto& bound : m_dialogs) { if (bound == dialog) { return; } }
    m_dialogs.append(dialog);
    dialog->setCoreTargets(&m_store);
    dialog->setCoreSettingsContext(currentContext());
    const QPointer<CoreSettingsHost> self(this);
    dialog->setCoresPageBinder([self](CoresSetupPage* page) { if (self) { self->bindPage(page); } });
}
void CoreSettingsHost::bindPage(CoresSetupPage* page)
{
    if (!page || m_retiring || m_pages.contains(page)) { return; }
    m_pages.insert(page, PageState{page, {}, {}});
    connect(page, &CoresSetupPage::inspectedTargetChanged, this, [this, page] { refreshPage(page); });
    connect(page, &CoresSetupPage::renameRequested, this,
            [this, page](const CoreRenameRequest& request) { requestRename(page, request); });
    connect(page, &CoresSetupPage::renameCancelled, this,
            [this, page](quint64 operationId) { cancelPage(page, operationId); });
    connect(page, &QObject::destroyed, this, [this, page] {
        cancelPage(page);
        m_pages.remove(page);
    });
    refreshPage(page);
}
void CoreSettingsHost::refreshPage(CoresSetupPage* page)
{
    auto found = m_pages.find(page);
    if (m_retiring || found == m_pages.end() || !found->page) { return; }
    CoreSettingsContext context = currentContext();
    context.renameTargetId = page->inspectedId();
    context.renameIncarnation = m_store.targetIncarnation(context.renameTargetId);
    const auto target = m_store.target(context.renameTargetId);
    if (target) { context.renamePairedIdentity = target->connection.identityFingerprint; }
    const auto snapshot = m_sessions ? m_sessions(context.renamePairedIdentity) : CoreRenameController::Snapshot{};
    const bool key = m_identity && m_identity->isValid() && snapshot.deviceIdentity == m_identity->fingerprint();
    const bool paired = target && context.renameIncarnation != 0
        && context.renamePairedIdentity.size() == 32 && !target->connection.allowUnpinned;
    const bool borrowed = key && snapshot.client && snapshot.client->isHandshakeComplete()
        && snapshot.client->signedInWithDeviceKey()
        && snapshot.client->stationIdentityFingerprint() == context.renamePairedIdentity
        && snapshot.client->deviceIdentityFingerprint() == m_identity->fingerprint()
        && snapshot.client->deviceAdminAvailable();
    const bool ownPending = m_active && m_active->page == page;
    const bool closing = ownsTemporaryAdmission(context.renamePairedIdentity) && !ownPending;
    const bool reachable = target && GuiConnectionController::isReadyToConnect(
        GuiConnectionController::connectionOptionsForTarget(*target));
    context.renameAvailable = paired && key && !closing && (borrowed || (!snapshot.sameCoreActive && reachable));
    if (!paired) { context.renameReason = tr("Pair with this Core again first, or recover its saved entry."); }
    else if (!key) { context.renameReason = tr("This computer's existing device key has not been loaded by an ordinary Core connection."); }
    else if (closing) { context.renameReason = admissionUnavailableReason(context.renamePairedIdentity); }
    else if (snapshot.sameCoreActive && !borrowed) {
        context.renameReason = snapshot.client && snapshot.client->isHandshakeComplete()
            ? snapshot.client->deviceAdminUnavailableReason()
            : tr("This Core already has a desktop connection or retry. Wait for it or disconnect it before renaming.");
    } else if (!reachable && !borrowed) { context.renameReason = tr("No retained listener or available remote access route can reach this paired Core."); }
    QByteArray authority;
    QDataStream stream(&authority, QIODevice::WriteOnly);
    stream << context.renameTargetId << context.renamePairedIdentity << context.renameIncarnation
        << snapshot.generation << quint64(reinterpret_cast<quintptr>(snapshot.client.data()))
        << (snapshot.client ? snapshot.client->sessionEpoch() : 0) << context.renameAvailable;
    if (found->authority != authority) {
        found->authority = authority;
        context.renameEpoch = nextAuthorityEpoch();
    } else { context.renameEpoch = found->context.renameEpoch; }
    found->context = context; // Install authority before setContext can synchronously cancel its operation.
    page->setContext(context);
}
void CoreSettingsHost::refresh()
{
    if (m_retiring) { return; }
    const auto pages = m_pages.keys();
    for (CoresSetupPage* page : pages) { refreshPage(page); }
    const CoreSettingsContext context = currentContext();
    m_dialogs.removeIf([](const QPointer<SetupDialog>& dialog) { return dialog.isNull(); });
    // Lazy factories get fresh current facts; per-page rename authority remains page-specific.
    for (const auto& dialog : m_dialogs) {
        if (dialog && !dialog->findChild<CoresSetupPage*>()) { dialog->setCoreSettingsContext(context); }
    }
}
bool CoreSettingsHost::requestCurrent(const CoreRenameController::Request& request) const
{
    if (m_retiring || !m_active || !m_active->page || helperRequest(m_active->request) != request) { return false; }
    const auto found = m_pages.constFind(m_active->page.data());
    if (found == m_pages.cend() || !found->page->isVisible()) { return false; }
    const auto& context = found->context;
    const auto target = m_store.target(request.targetId);
    return target && context.renameAvailable && context.renameEpoch == request.epoch
        && context.renameTargetId == request.targetId && context.renamePairedIdentity == request.pairedIdentity
        && request.incarnation != 0 && m_store.targetIncarnation(request.targetId) == request.incarnation
        && found->page->inspectedId() == request.targetId
        && found->page->inspectedIncarnation() == request.incarnation
        && target->connection.identityFingerprint == request.pairedIdentity && !target->connection.allowUnpinned;
}
void CoreSettingsHost::requestRename(CoresSetupPage* page, const CoreRenameRequest& request)
{
    refreshPage(page);
    if (!m_rename) { page->finishRename(request, false, tr("This computer's existing device key is unavailable.")); return; }
    const Route route{page, request};
    const auto previous = m_active;
    m_active = route;
    if (!requestCurrent(helperRequest(request))) {
        m_active = previous; // An invalid request in another page cannot cancel the origin's work.
        page->finishRename(request, false, tr("The inspected Core's rename authority changed. Open it again."));
        return;
    }
    m_routes.append(route); // Full tuple includes the originating page's unique authority epoch.
    m_rename->cancel();
    m_rename->inspectTarget(request.targetId);
    m_rename->rename(helperRequest(request));
}
void CoreSettingsHost::cancelPage(CoresSetupPage* page, quint64 operationId)
{
    if (m_active && m_active->page == page
        && (operationId == 0 || m_active->request.operationId == operationId)) {
        if (m_rename) { m_rename->cancel(); }
    }
}
void CoreSettingsHost::retireBindings()
{
    m_retiring = true;
    for (const auto& dialog : m_dialogs) { if (dialog) { dialog->setCoresPageBinder({}); } }
    for (const PageState& state : m_pages) {
        if (state.page) {
            disconnect(state.page, nullptr, this, nullptr);
            state.page->cancelOperations();
            CoreSettingsContext retired;
            retired.renameReason = tr("The Settings host for this window has closed.");
            retired.renameEpoch = nextAuthorityEpoch();
            state.page->setContext(retired);
        }
    }
    if (m_rename) { m_rename->cancel(); }
    m_active.reset();
    m_routes.clear();
    m_pages.clear();
    m_dialogs.clear();
    m_currentAuthority.clear();
    m_contextSourceAuthority.clear();
    m_contextSourceIncarnation = 0;
    m_retiring = false;
}
} // namespace NereusSDR
