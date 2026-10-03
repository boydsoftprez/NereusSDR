#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// src/gui/multidevice/NoticeCard.h  (NereusSDR)
// =================================================================
//
// A card on the band for one notice (iPhone app plan Task 78, R-IOS-30;
// the several-devices design, sections 7.4 and 12 item 3): what another
// device did (or what happened to this window's own slices), when, and
// Take it back where the Core offers it (`takeBack`). graceEnded,
// slicesNotRestored and antennaKept use the same card with nothing to
// answer. Close puts the card away. A Take it back the notice does not
// offer but the card is given a reason for (`takeBackOff`) is shown off,
// with that reason: disabled, never hidden.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-26: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), iPhone app plan Task 78 (R-IOS-30), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-30: take-over parity: a Take it back the Core cannot run here
//               is shown off, with the reason as its tooltip. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/session/RemoteDevicesState.h"

#include <QFrame>

class QLabel;
class QPushButton;

namespace NereusSDR {

class NoticeCard : public QFrame {
    Q_OBJECT

public:
    explicit NoticeCard(const RemotePrompt& notice, QWidget* parent = nullptr,
                        const QString& takeBackOff = QString());

    qint64 noticeId() const { return m_id; }
    /// "At 14:05. " + the Core's reason.
    static QString text(const RemotePrompt& notice);

    QLabel* textLabel() const { return m_text; }
    /// Null when the notice has nothing to answer; disabled when it is
    /// shown off.
    QPushButton* takeBackButton() const { return m_takeBack; }
    QPushButton* closeButton() const { return m_close; }

signals:
    void takeBackRequested(qint64 id);
    void dismissed(qint64 id);

private:
    qint64 m_id = 0;
    QLabel* m_text = nullptr;
    QPushButton* m_takeBack = nullptr;
    QPushButton* m_close = nullptr;
};

} // namespace NereusSDR
