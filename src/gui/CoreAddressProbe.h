#pragma once
// no-port-check: NereusSDR-original.
// 2026-10-02 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.

#include "core/session/PathRacer.h"
#include <QTimer>

namespace NereusSDR {

/// Proves one explicitly entered listener without signing in or sending frames.
/// A factory supplies an owned rung; production always uses DirectPathRung.
class CoreAddressProbe : public QObject {
    Q_OBJECT
public:
    enum class Outcome { Verified, Refused, TimedOut, Cancelled };
    Q_ENUM(Outcome)
    using RungFactory = std::function<PathRung*(const QUrl&)>;
    static constexpr int kDeadlineMs = 10000;
    explicit CoreAddressProbe(QObject* parent = nullptr, RungFactory factory = {},
                              int deadlineMs = kDeadlineMs);
    ~CoreAddressProbe() override;
    void start(const QUrl& endpoint, const QByteArray& pairedIdentity);
    void cancel();
    bool pending() const { return m_pending; }
signals:
    void finished(NereusSDR::CoreAddressProbe::Outcome outcome, const QString& reason);
private:
    void finish(Outcome outcome, const QString& reason);
    void releaseRace();
    RungFactory m_factory;
    int m_deadlineMs;
    QTimer m_deadline;
    QPointer<PathRacer> m_racer;
    bool m_pending = false;
    quint64 m_startGeneration = 0;
};

} // namespace NereusSDR
