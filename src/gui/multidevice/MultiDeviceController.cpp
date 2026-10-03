// no-port-check: NereusSDR-original.
// =================================================================
// src/gui/multidevice/MultiDeviceController.cpp  (NereusSDR)
// =================================================================
//
// See MultiDeviceController.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-26: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), iPhone app plan Task 78 (R-IOS-02, R-IOS-30), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-28: the fifth-device choice (Task 78 item 7, G-53). J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-28: slice control and shared listening plan Task 5: the
//               controlTaken notice and the slice access refusals and holds
//               reach refusal(). J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-09-29: slice control plan Task 10: questionDialog() builds the
//               question's dialog, and stackNoticeCards() places the notice
//               cards, for the hosting desktop too. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30: take-over parity: controlTaken is a card with Take it
//               back (controlTakeBackOff shows it off on an older Core).
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30: desktop listening lane review: foreignMarkers() names the
//               hosting desktop (or the Core itself) for a slice the station
//               device holds, from the access entry. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-09-30: TX badge take fix round 1: lastTakeCommandId(), the
//               tx.take the window's own take question sent. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30: fix wave GUI-I1 and GUI-M6: a question or take dialog
//               closes when its session ends or a new one starts, and is
//               stamped with the session it was asked in; a notice card's
//               Take it back is shown off once its session ends. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "gui/multidevice/MultiDeviceController.h"

#include "core/session/RemoteDevicesState.h"
#include "core/session/SliceAccessMirror.h"
#include "core/session/StationClient.h"
#include "core/session/TransmitStateFacade.h"
#include "gui/multidevice/ConfirmChangeDialog.h"
#include "gui/multidevice/NoticeCard.h"
#include "gui/multidevice/ReplaceDeviceDialog.h"
#include "gui/multidevice/TakeReceiverDialog.h"
#include "gui/multidevice/TakeTransmitDialog.h"
#include "gui/widgets/VfoWidget.h"

#include <QDialog>
#include <QSet>
#include <QWidget>

#include <algorithm>
#include <functional>

namespace NereusSDR {

namespace {

constexpr int kCardMargin = 10;
constexpr int kCardGap = 6;
constexpr int kCardMaxWidth = 520;

} // namespace

MultiDeviceController::MultiDeviceController(StationClient* client, QWidget* dialogParent,
                                             QObject* parent)
    : QObject(parent)
    , m_client(client)
    , m_dialogParent(dialogParent)
{
    if (!m_client) {
        return;
    }
    RemoteDevicesState* devices = m_client->remoteDevices();
    connect(devices, &RemoteDevicesState::questionChanged, this,
            &MultiDeviceController::onQuestionChanged);
    connect(devices, &RemoteDevicesState::heldChanged, this,
            &MultiDeviceController::onHeldChanged);
    connect(devices, &RemoteDevicesState::noticesChanged, this,
            &MultiDeviceController::onNoticesChanged);
    connect(devices, &RemoteDevicesState::markersChanged, this,
            &MultiDeviceController::markersChanged);
    connect(m_client, &StationClient::deviceCommandFinished, this,
            &MultiDeviceController::onCommandFinished);
    // Fix wave GUI-I1: a dialog answers the session it was asked in. The
    // session ending closes it; a redial that replaces the link silently
    // (no sessionEnded) is caught at the new session's handshake and by
    // the epoch each answer checks before it is sent.
    connect(m_client, &StationClient::sessionEnded, this,
            &MultiDeviceController::onSessionEnded);
    connect(m_client, &StationClient::handshakeComplete, this, [this]() {
        if (m_dialog && m_dialogEpoch != currentEpoch()) {
            closeDialogQuietly();
        }
        refreshCardsForSession();
    });
    // Slice control plan Task 5: a change held back on a slice this window
    // only listens to says why, as the Core's refusal would.
    connect(m_client, &StationClient::sliceAccessHeld, this,
            [this](int, const QString& reason) {
                if (!reason.isEmpty()) {
                    emit refusal(reason);
                }
            });
}

MultiDeviceController::~MultiDeviceController()
{
    // A card with no band to sit on yet has no parent to delete it.
    for (const QPointer<NoticeCard>& card : m_cards) {
        if (card && card->parent() == nullptr) {
            delete card.data();
        }
    }
    closeDialogQuietly();
}

QList<NoticeCard*> MultiDeviceController::noticeCards() const
{
    QList<NoticeCard*> cards;
    for (const QPointer<NoticeCard>& card : m_cards) {
        if (card) {
            cards.append(card.data());
        }
    }
    return cards;
}

void MultiDeviceController::showDialog(QDialog* dialog)
{
    closeDialogQuietly();
    m_dialog = dialog;
    m_dialogEpoch = currentEpoch();  // fix wave GUI-I1
    dialog->setAttribute(Qt::WA_DeleteOnClose, true);
    // Window-modal and not blocking: the Core's next message (a newer
    // question, the answer) still arrives while the operator reads.
    dialog->open();
}

void MultiDeviceController::closeDialogQuietly()
{
    if (!m_dialog) {
        return;
    }
    QPointer<QDialog> dialog = m_dialog;
    m_dialog.clear();
    m_dialogQuestionId = 0;
    m_dialogIsHeld = false;
    // Closed without answering: its accepted/rejected handlers check that
    // they are still the open dialog.
    dialog->close();
}

void MultiDeviceController::askTakeTransmit()
{
    if (!m_client || !m_client->transmitHeldElsewhere()) {
        return;
    }
    const TransmitState& tx = *m_client->transmitState();
    if (tx.holderTransferring()) {
        emit refusal(QStringLiteral("Transmit is changing hands. Try again in a moment."));
        return;
    }
    // What the operator is shown is what the take carries (ruling 8.7): the
    // Core takes at once only if nothing changed since.
    const qint64 shownEpoch = tx.holderEpoch();
    const bool shownKeyed = tx.keyed();
    auto* dialog = new TakeTransmitDialog(TakeTransmitDialog::fromTransmitState(tx),
                                          m_dialogParent);
    const QPointer<QDialog> self(dialog);
    const quint32 askedEpoch = currentEpoch();
    connect(dialog, &QDialog::accepted, this,
            [this, self, shownEpoch, shownKeyed, askedEpoch]() {
        if (m_dialog != self) { return; }
        m_dialog.clear();
        m_lastTakeCommandId = 0;
        // Fix wave GUI-I1: what was shown belongs to the session it was
        // shown in; a newer Core's holder epoch can restart at the same
        // number, so the take would cut a holder never shown.
        if (m_client && askedEpoch == currentEpoch()) {
            m_lastTakeCommandId = m_client->requestTakeTransmit(true, shownEpoch, shownKeyed);
        }
    });
    connect(dialog, &QDialog::rejected, this, [this, self]() {
        if (m_dialog == self) { m_dialog.clear(); }
    });
    showDialog(dialog);
    m_dialogQuestionId = 0;
}

QDialog* MultiDeviceController::questionDialog(const SessionPrompt& prompt,
                                               QWidget* parent,
                                               std::function<qint64()>* choice)
{
    std::function<qint64()> picked = []() { return qint64(-1); };
    QDialog* dialog = nullptr;
    if (prompt.kind == QStringLiteral("takeTransmit")) {
        dialog = new TakeTransmitDialog(
            TakeTransmitDialog::fromHolderEntry(prompt.holder.value_or(QJsonObject{})),
            parent);
    } else if (prompt.kind == QStringLiteral("takeReceiver")
               || prompt.kind == QStringLiteral("takeSlice")) {
        auto* chooser = new TakeReceiverDialog(prompt, parent);
        const QPointer<TakeReceiverDialog> guard(chooser);
        picked = [guard]() { return guard ? guard->pickedChoice() : qint64(-1); };
        dialog = chooser;
    } else {
        // sharedSetting, panMove, and any kind a newer Core adds: the one
        // shape that shows the change and who it reaches.
        dialog = new ConfirmChangeDialog(prompt, parent);
    }
    if (choice != nullptr) {
        *choice = std::move(picked);
    }
    return dialog;
}

void MultiDeviceController::onQuestionChanged()
{
    if (!m_client) {
        return;
    }
    const std::optional<RemotePrompt> question = m_client->remoteDevices()->question();
    if (!question) {
        // Answered, or dropped: a dialog for it has nothing left to ask.
        if (m_dialog && m_dialogQuestionId != 0) {
            closeDialogQuietly();
        }
        return;
    }
    if (m_dialog && m_dialogQuestionId == question->prompt.id) {
        return;
    }
    const SessionPrompt& prompt = question->prompt;
    const qint64 id = prompt.id;
    const quint32 askedEpoch = currentEpoch();  // fix wave GUI-I1
    std::function<qint64()> choice;
    QDialog* dialog = questionDialog(prompt, m_dialogParent, &choice);
    const QPointer<QDialog> self(dialog);
    connect(dialog, &QDialog::accepted, this, [this, self, id, choice, askedEpoch]() {
        if (m_dialog != self) { return; }
        const qint64 picked = choice();
        m_dialog.clear();
        m_dialogQuestionId = 0;
        if (m_client && askedEpoch == currentEpoch()) {
            m_client->proceedQuestion(id, picked);
        }
    });
    connect(dialog, &QDialog::rejected, this, [this, self, id, askedEpoch]() {
        if (m_dialog != self) { return; }
        m_dialog.clear();
        m_dialogQuestionId = 0;
        if (m_client && askedEpoch == currentEpoch()) {
            m_client->cancelQuestion(id);
        }
    });
    showDialog(dialog);
    m_dialogQuestionId = id;
}

void MultiDeviceController::onHeldChanged()
{
    if (!m_client) {
        return;
    }
    const std::optional<RemoteHeldList> held = m_client->remoteDevices()->held();
    if (!held) {
        // Answered, or the session moved on or ended.
        if (m_dialog && m_dialogIsHeld) {
            closeDialogQuietly();
        }
        return;
    }
    if (m_dialog && m_dialogIsHeld) {
        // A newer list while the operator reads: the rows change in place.
        if (auto* open = qobject_cast<ReplaceDeviceDialog*>(m_dialog.data())) {
            open->setHeld(*held);
            return;
        }
    }
    auto* dialog = new ReplaceDeviceDialog(*held, m_dialogParent);
    const QPointer<ReplaceDeviceDialog> self(dialog);
    const quint32 askedEpoch = currentEpoch();  // fix wave GUI-I1
    connect(dialog, &QDialog::accepted, this, [this, self, askedEpoch]() {
        if (m_dialog != self) { return; }
        const QString picked = self->pickedDeviceId();
        m_dialog.clear();
        m_dialogIsHeld = false;
        if (m_client && askedEpoch == currentEpoch()) {
            m_client->answerHeld(picked);
        }
    });
    connect(dialog, &QDialog::rejected, this, [this, self, askedEpoch]() {
        if (m_dialog != self) { return; }
        m_dialog.clear();
        m_dialogIsHeld = false;
        if (m_client && askedEpoch == currentEpoch()) {
            m_client->answerHeld(QString());
        }
    });
    showDialog(dialog);
    m_dialogIsHeld = true;
}

void MultiDeviceController::onNoticesChanged()
{
    if (!m_client) {
        return;
    }
    // Take-over parity: controlTaken (another device took control of a
    // slice this window controlled) is a card like any other notice, with
    // Take it back; an older Core offers none, and the button is shown off
    // with the reason.
    const QList<RemotePrompt> notices = m_client->remoteDevices()->notices();
    const bool takesControlBack = m_client->controlTakeBackAvailable();
    QSet<qint64> live;
    for (const RemotePrompt& notice : notices) {
        live.insert(notice.prompt.id);
        QPointer<NoticeCard>& card = m_cards[notice.prompt.id];
        if (card) {
            continue;
        }
        card = new NoticeCard(notice, m_noticeHost, controlTakeBackOff(notice, takesControlBack));
        // Fix wave GUI-M6: the card's Take it back answers the session the
        // notice came in, never a later one (whose ids start over).
        const quint32 cardEpoch = currentEpoch();
        m_cardEpochs.insert(notice.prompt.id, cardEpoch);
        connect(card, &NoticeCard::takeBackRequested, this, [this, cardEpoch](qint64 id) {
            if (!m_client) {
                return;
            }
            if (cardEpoch != currentEpoch() || cardEpoch == m_endedEpoch) {
                emit refusal(sessionEndedTakeBackReason());
                return;
            }
            m_client->takeBackNotice(id);
        });
        connect(card, &NoticeCard::dismissed, this, [this](qint64 id) {
            if (m_client) {
                m_client->remoteDevices()->dismissNotice(id);
            }
        });
    }
    for (auto it = m_cards.begin(); it != m_cards.end();) {
        if (!live.contains(it.key()) || !it.value()) {
            if (it.value()) {
                it.value()->deleteLater();
                it.value()->hide();
            }
            m_cardEpochs.remove(it.key());
            it = m_cards.erase(it);
        } else {
            ++it;
        }
    }
    refreshCardsForSession();
    layoutNoticeCards();
}

quint32 MultiDeviceController::currentEpoch() const
{
    return m_client ? m_client->sessionEpoch() : 0;
}

QString MultiDeviceController::sessionEndedTakeBackReason()
{
    return QStringLiteral("The connection to the Core ended. This can no longer be taken back.");
}

void MultiDeviceController::onSessionEnded()
{
    // Fix wave GUI-I1: a question or take asked in the session that ended
    // has nothing left to answer.
    m_endedEpoch = currentEpoch();
    closeDialogQuietly();
    refreshCardsForSession();
}

void MultiDeviceController::refreshCardsForSession()
{
    // Fix wave GUI-M6: a card stays to be read after its session ends (the
    // notices outlive the link, RemoteDevicesState::clear), but its Take it
    // back is shown off with the reason.
    for (auto it = m_cards.cbegin(); it != m_cards.cend(); ++it) {
        NoticeCard* card = it.value().data();
        if (!card || !card->takeBackButton()) {
            continue;
        }
        const quint32 cardEpoch = m_cardEpochs.value(it.key());
        const bool stale = cardEpoch != currentEpoch() || cardEpoch == m_endedEpoch;
        if (stale && card->takeBackButton()->isEnabled()) {
            card->takeBackButton()->setEnabled(false);
            card->takeBackButton()->setToolTip(sessionEndedTakeBackReason());
            card->takeBackButton()->setAccessibleDescription(sessionEndedTakeBackReason());
        }
    }
}

QString MultiDeviceController::controlTakeBackOff(const RemotePrompt& notice, bool available)
{
    if (notice.prompt.kind != QStringLiteral("controlTaken") || notice.prompt.takeBack) {
        return {};
    }
    // A Core that can run it and did not offer it: the take-back ended
    // (its device was away past its 3 minutes).
    return available ? QStringLiteral("That can no longer be taken back.")
                     : StationClient::controlTakeBackUnavailableReason();
}

void MultiDeviceController::setNoticeHost(QWidget* host)
{
    if (m_noticeHost == host) {
        return;
    }
    m_noticeHost = host;
    for (const QPointer<NoticeCard>& card : m_cards) {
        if (card) {
            card->setParent(host);
        }
    }
    layoutNoticeCards();
}

void MultiDeviceController::layoutNoticeCards()
{
    stackNoticeCards(m_noticeHost, noticeCards());
}

void MultiDeviceController::stackNoticeCards(QWidget* host, QList<NoticeCard*> cards)
{
    if (!host) {
        return;
    }
    // Stacked from the foot of the band upwards, newest at the foot.
    const int width = std::min(kCardMaxWidth, host->width() - 2 * kCardMargin);
    int bottom = host->height() - kCardMargin;
    std::sort(cards.begin(), cards.end(), [](const NoticeCard* a, const NoticeCard* b) {
        return a->noticeId() > b->noticeId();
    });
    for (NoticeCard* card : cards) {
        const int h = card->heightForWidth(width) > 0 ? card->heightForWidth(width)
                                                      : card->sizeHint().height();
        bottom -= h;
        card->setGeometry(kCardMargin, std::max(kCardMargin, bottom), std::max(120, width), h);
        card->show();
        card->raise();
        bottom -= kCardGap;
    }
}

void MultiDeviceController::onCommandFinished(const QByteArray& verb, quint32 commandId,
                                              bool accepted, const QString& reason,
                                              bool awaitingConfirmation)
{
    Q_UNUSED(commandId)
    if (awaitingConfirmation) {
        // The Core's question follows; it is shown when it arrives.
        return;
    }
    if (accepted) {
        if (verb == "tx.take"
            || (verb == "confirm.proceed" && m_client && m_client->holdsTransmitHere())) {
            emit transmitTaken();
        }
        return;
    }
    if (verb == "session.leave" || verb == "confirm.cancel") {
        return;
    }
    if (!reason.isEmpty()) {
        emit refusal(reason);
    }
}

QVector<SpectrumWidget::ForeignSliceMarker> MultiDeviceController::foreignMarkers(
    const RemoteDevicesState& devices, const SliceAccessMirror* access)
{
    const QString station = QStringLiteral("station");
    QVector<SpectrumWidget::ForeignSliceMarker> out;
    for (const RemoteSliceMarker& m : devices.markers()) {
        SpectrumWidget::ForeignSliceMarker f;
        f.sliceId = m.sliceId;
        f.centreHz = m.frequencyHz;
        f.filterLowHz = m.filterLowHz;
        f.filterHighHz = m.filterHighHz;
        // A marker's colour is its letter's (the several-devices design,
        // section 5.4); the fifth letter shares A's, the label tells them
        // apart.
        f.color = VfoWidget::sliceColor(m.sliceId);
        f.letter = m.letter();
        f.ownerShortName = m.ownerShortName;
        f.ownerName = m.ownerName;
        f.tx = m.txSlice;
        f.away = m.ownerAway;
        if (m.ownerDeviceId.isEmpty()) {
            // The Core sends the same marker for a slice the station device
            // holds and for one nobody holds; the access entry decides.
            const std::optional<SliceAccessMirror::Entry> entry =
                access ? access->entry(m.sliceId) : std::nullopt;
            if (entry && entry->controllerDeviceId == station) {
                if (const auto host = devices.sliceHolderDevice(station)) {
                    f.ownerName = host->name;
                    f.ownerShortName = host->shortName;
                } else {
                    // A Core no desktop hosts, in the chooser's words.
                    f.ownerName = QStringLiteral("the Core itself");
                    f.ownerShortName = QStringLiteral("Core");
                }
            } else {
                f.unowned = true;
            }
        }
        out.append(f);
    }
    return out;
}

} // namespace NereusSDR
