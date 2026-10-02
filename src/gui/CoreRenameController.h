#pragma once
// no-port-check: NereusSDR-original.
// 2026-10-02 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#include "gui/CoreTargetStore.h"
#include "core/session/StationClient.h"
#include <QPointer>
#include <QTimer>
#include <memory>
namespace NereusSDR {
class ClientDeviceIdentity;
/// Renaming uses ordinary device admission, with temporary-session side effects.
/// The host supplies the existing device key, shared desktop exclusion and exact
/// inspected-page authority. The store must outlive this helper.
/// Use one app-scoped helper for every Settings dialog. SessionSource reports
/// desktop sessions, excluding this helper's own current temporary admission.
class CoreRenameController final : public QObject {
    Q_OBJECT
public:
    struct Request {
        quint64 operationId = 0;
        QString targetId;
        QByteArray pairedIdentity;
        quint64 incarnation = 0;
        quint64 epoch = 0; // Inspected UI authority; distinct from coordinator/client epochs.
        QString name;
        bool operator==(const Request&) const = default;
    };
    struct Snapshot {
        QPointer<StationClient> client;
        bool sameCoreActive = false; // Includes pending/service/race/retry.
        quint64 generation = 0;
        QByteArray deviceIdentity;
    };
    struct Limits { int admissionMs; int commandMs; int nameMs; int cleanupMs; };
    enum class Outcome { Accepted, Refused, TimedOut, UnknownOutcome, Cancelled, StaleTarget, AcceptedLocalSaveFailed };
    Q_ENUM(Outcome)
    using SessionSource = std::function<Snapshot(const QByteArray&)>;
    using RequestAuthority = std::function<bool(const Request&)>;
    using TemporaryFactory = std::function<std::unique_ptr<StationClient>()>;
    using Starter = std::function<void(StationClient&, const RemoteStationOptions&)>;
    CoreRenameController(CoreTargetStore& store, SessionSource source,
                         std::shared_ptr<const ClientDeviceIdentity> identity,
                         RequestAuthority authority, QObject* parent = nullptr,
                         TemporaryFactory factory = {}, Starter starter = {},
                         Limits limits = {30000, 10000, 2000, 500});
    ~CoreRenameController() override;
    void inspectTarget(const QString& id);
    void rename(const Request& request);
    void cancel();
    bool pending() const { return m_operation.has_value(); }
    /// Ordinary Connect must respect this lease through temporary close retirement.
    bool ownsTemporaryAdmission(const QByteArray& pairedIdentity) const;
signals:
    /// Terminal notification is queued; cleanup/retirement precedes callbacks.
    void finished(NereusSDR::CoreRenameController::Request request,
                  NereusSDR::CoreRenameController::Outcome outcome,
                  const QString& acceptedName, const QString& reason);
private:
    struct Operation {
        Request request;
        RemoteStationOptions trust;
        quint64 generation = 0;
        quint64 coordinatorGeneration = 0;
        QPointer<StationClient> client;
        quint32 clientEpoch = 0;
        quint32 commandId = 0;
        bool temporary = false;
        bool accepted = false;
        QString normalizedName;
        QString baselineName;
        QString observedName;
        QList<QMetaObject::Connection> connections;
    };
    bool current(const Operation& operation, bool requireClient = false) const;
    bool admissionAllowed(quint64 generation) const;
    void bindClient();
    void sendRename();
    void observeName();
    void acceptedName();
    void finish(Outcome outcome, const QString& name, const QString& reason);
    void notify(const Request& request, Outcome outcome, const QString& name, const QString& reason);
    void cleanup(StationClient* client, const QByteArray& pairedIdentity);
    CoreTargetStore& m_store;
    SessionSource m_source;
    std::shared_ptr<const ClientDeviceIdentity> m_identity;
    RequestAuthority m_authority;
    TemporaryFactory m_factory;
    Starter m_starter;
    Limits m_limits;
    QString m_inspectedId;
    quint64 m_inspectedIncarnation = 0;
    quint64 m_generation = 0;
    QTimer m_timer;
    std::unique_ptr<StationClient> m_temporary;
    struct Drain { QPointer<StationClient> client; QByteArray pairedIdentity; };
    QList<Drain> m_draining;
    std::optional<Operation> m_operation;
};
} // namespace NereusSDR
Q_DECLARE_METATYPE(NereusSDR::CoreRenameController::Request)
