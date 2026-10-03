#pragma once
// no-port-check: NereusSDR-original Core Settings host and page authority.
// 2026-10-02 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#include "gui/CoreRenameController.h"
#include "gui/setup/CoreSettingsContext.h"
#include <QMap>
#include <QPointer>
namespace NereusSDR {
class SetupDialog;
class CoresSetupPage;
/// One host per application. The canonical store outlives this host and pages.
/// Sources supply the owning window only; inspection does not select or dial.
class CoreSettingsHost final : public QObject {
    Q_OBJECT
public:
    using ContextSource = std::function<CoreSettingsContext()>;
    CoreSettingsHost(CoreTargetStore& store, ContextSource context,
                     CoreRenameController::SessionSource sessions, QObject* parent = nullptr,
                     CoreRenameController::TemporaryFactory factory = {},
                     CoreRenameController::Starter starter = {},
                     CoreRenameController::Limits limits = {30000, 10000, 2000, 500});
    ~CoreSettingsHost() override;
    void setExistingDeviceIdentity(std::shared_ptr<const ClientDeviceIdentity> identity);
    void bindDialog(SetupDialog* dialog);
    void bindPage(CoresSetupPage* page);
    void refresh();
    void retireBindings();
    bool ownsTemporaryAdmission(const QByteArray& identity) const;
    QString admissionUnavailableReason(const QByteArray& identity) const;
signals:
    void admissionChanged();
    void coreNamePresentationChanged();
private:
    struct PageState {
        QPointer<CoresSetupPage> page;
        QByteArray authority;
        CoreSettingsContext context;
    };
    struct Route { QPointer<CoresSetupPage> page; CoreRenameRequest request; };
    CoreSettingsContext currentContext();
    void refreshPage(CoresSetupPage* page);
    void requestRename(CoresSetupPage* page, const CoreRenameRequest& request);
    void cancelPage(CoresSetupPage* page, quint64 operationId = 0);
    bool requestCurrent(const CoreRenameController::Request& request) const;
    CoreRenameController::Snapshot sessionSnapshot(const QByteArray& identity) const;
    CoreTargetStore& m_store;
    ContextSource m_contextSource;
    CoreRenameController::SessionSource m_sessions;
    CoreRenameController::TemporaryFactory m_factory;
    CoreRenameController::Starter m_starter;
    CoreRenameController::Limits m_limits;
    std::shared_ptr<const ClientDeviceIdentity> m_identity;
    std::unique_ptr<CoreRenameController> m_rename;
    QList<QPointer<SetupDialog>> m_dialogs;
    QMap<CoresSetupPage*, PageState> m_pages;
    QList<Route> m_routes;
    std::optional<Route> m_active;
    QByteArray m_currentAuthority;
    QByteArray m_contextSourceAuthority;
    quint64 m_contextSourceIncarnation = 0;
    quint64 m_currentEpoch = 0;
    bool m_retiring = false;
};
} // namespace NereusSDR
