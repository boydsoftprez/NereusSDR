#pragma once
// no-port-check: NereusSDR-original.
// 2026-10-02 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#include "gui/CoreAddressProbe.h"
#include "gui/CoreTargetStore.h"

namespace NereusSDR {

/// Inspection never selects or connects a Core. Only deliberate Add/Edit probes.
/// The store must outlive this controller. Call cancel() on Back/page closure.
class CoreAddressController : public QObject {
    Q_OBJECT
public:
    enum class Outcome { Saved, Refused, TimedOut, Cancelled, StaleTarget, SaveFailed };
    Q_ENUM(Outcome)
    explicit CoreAddressController(CoreTargetStore& store, QObject* parent = nullptr,
                                   CoreAddressProbe::RungFactory factory = {},
                                   int deadlineMs = CoreAddressProbe::kDeadlineMs);
    ~CoreAddressController() override;
    void inspectTarget(const QString& id);
    QString inspectedId() const { return m_inspectedId; }
    bool pending() const { return m_operation.has_value(); }
    quint64 addAddress(const QString& hostOrEndpoint, int port);
    quint64 editAddress(const QString& previousAddress, const QString& hostOrEndpoint, int port);
    bool removeAddress(const QString& address, QString* error = nullptr);
    void cancel();
signals:
    /// Final outcomes (including immediate validation refusal) are queued.
    void finished(quint64 operationId, NereusSDR::CoreAddressController::Outcome outcome,
                  const QString& reason);
private:
    struct Operation {
        quint64 id = 0;
        QString targetId;
        quint64 incarnation = 0;
        QByteArray identity;
        QString address;
        std::optional<QString> previousAddress;
    };
    quint64 begin(const QString& hostOrEndpoint, int port, std::optional<QString> previousAddress);
    void checked(CoreAddressProbe::Outcome outcome, const QString& reason);
    void finish(Outcome outcome, const QString& reason);
    void refuseLater(Outcome outcome, const QString& reason);
    bool current(const Operation& operation) const;
    CoreTargetStore& m_store;
    CoreAddressProbe m_probe;
    QString m_inspectedId;
    quint64 m_inspectedIncarnation = 0;
    quint64 m_nextOperation = 0;
    std::optional<Operation> m_operation;
};
} // namespace NereusSDR
