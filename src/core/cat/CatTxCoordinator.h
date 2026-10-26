// no-port-check: NereusSDR-original CAT accepted-activation ownership.
// 2026-10-04 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
// 2026-10-06 - Export the class from the Windows Core DLL so the GUI and tests
//              can use its signals. J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
// 2026-10-26 - A CAT TX that arrives while MoxController walks back to
//              receive is held and runs again at Rx (JJ's ruling, like
//              Thetis): one held request, cancelled by a release, a newer
//              request, its session closing or another keyer, and dropped
//              past the walk's own bound. J.J. Boyd (KG4VCF), AI-assisted
//              via Anthropic Claude Code.
// 2026-10-26 - Review fixes: a stop drops a held TX and refuses a new one;
//              the bound is MoxController's remaining walk; a stale queued
//              replay is ignored; releasePttClaim for an idle serial pin.
//              J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
#pragma once
#include "core/NereusCoreExport.h"
#include "core/MoxController.h"
#include "core/SliceOwnership.h"
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QTimer>
#include <functional>
namespace NereusSDR {
class RadioModel;
enum class CatTransmitKind { Ptt, Tune, TwoTone };
class NEREUS_CORE_EXPORT CatTxCoordinator : public QObject {
    Q_OBJECT
public:
    explicit CatTxCoordinator(RadioModel& model);
    bool requestPtt(quint64 sessionId, int sliceId);
    void releasePtt(quint64 sessionId);
    // An idle serial PTT pin: releases that session's PTT claim, as
    // releasePtt does, but leaves a held TX alone. Thetis ORs the pin
    // into its CAT PTT level (console.cs:25476-25477 [v2.10.3.15]), so an
    // idle pin never clears a CAT TX; only the pin's release edge does.
    void releasePttClaim(quint64 sessionId);
    bool requestTxSelection(quint64 sessionId, int sliceId);
    bool requestTransmit(quint64 sessionId, int sliceId, CatTransmitKind kind);
    // A PTT request that arrives while MoxController walks back from transmit
    // to receive (TxToRxInFlight, TxToRxFlush) is held, and true is returned.
    // At Rx, replayAtRx runs the caller's whole command again for the same
    // session, so the held request meets every check a fresh one would.
    // Otherwise this is requestTransmit above.
    bool requestTransmit(quint64 sessionId, int sliceId, CatTransmitKind kind,
                         std::function<void()> replayAtRx);
    void releaseTransmit(quint64 sessionId);
    void releaseTransmit(quint64 sessionId, CatTransmitKind kind);
    void cancelSession(quint64 sessionId);
    void cancelAll();
    quint64 currentRequestTag() const { return m_tag; }
    // FOR TESTING ONLY: the held TX's bound
    // (MoxController::txToRxRemainingMaxMs at hold time).
    void setHeldTxTimeoutMsForTest(int ms);
private:
    bool idle() const;
    bool targetValid() const;
    void retire(bool releaseOwned);
    bool mayHold() const;
    void dropHeld(const char* reason);
    void runHeld(quint64 serial);
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
    int m_heldTimeoutForTestMs{-1};
    quint64 m_heldSession{0};
    quint64 m_heldSerial{0};
    std::function<void()> m_heldReplay;
    QTimer m_heldTimer;
    bool m_replaying{false};
};
} // namespace NereusSDR
