// no-port-check: NereusSDR-original CAT accepted-activation ownership.
// 2026-10-04 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#pragma once
#include "core/MoxController.h"
#include "core/SliceOwnership.h"
#include <QObject>
#include <QPointer>
#include <QSet>
namespace NereusSDR {
class RadioModel;
enum class CatTransmitKind { Ptt, Tune, TwoTone };
class CatTxCoordinator : public QObject {
    Q_OBJECT
public:
    explicit CatTxCoordinator(RadioModel& model);
    bool requestPtt(quint64 sessionId, int sliceId);
    void releasePtt(quint64 sessionId);
    bool requestTxSelection(quint64 sessionId, int sliceId);
    bool requestTransmit(quint64 sessionId, int sliceId, CatTransmitKind kind);
    void releaseTransmit(quint64 sessionId);
    void releaseTransmit(quint64 sessionId, CatTransmitKind kind);
    void cancelSession(quint64 sessionId);
    void cancelAll();
    quint64 currentRequestTag() const { return m_tag; }
private:
    bool idle() const;
    bool targetValid() const;
    void retire(bool releaseOwned);
    QPointer<RadioModel> m_model;
    QSet<quint64> m_claims;
    SliceOwnership::SliceRef m_target;
    quint64 m_controlRevision{0};
    quint64 m_nextTag{0};
    quint64 m_tag{0};
    quint64 m_stamp{0};
    CatTransmitKind m_kind{CatTransmitKind::Ptt};
    bool m_requestInFlight{false};
    bool m_selectionInFlight{false};
    bool m_releasing{false};
};
} // namespace NereusSDR
