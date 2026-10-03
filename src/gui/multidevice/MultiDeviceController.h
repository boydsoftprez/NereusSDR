#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// src/gui/multidevice/MultiDeviceController.h  (NereusSDR)
// =================================================================
//
// The remote window's half of several devices on one Core (iPhone app plan
// Task 78, R-IOS-02, R-IOS-30; the several-devices design, section 12).
// One per remote window, owned by MainWindow. It turns what the Core says
// (StationClient::remoteDevices(): the question, the notices, the markers,
// txState's holder) into the window's screens, and the operator's answers
// into the Core's verbs:
//
//   askTakeTransmit()   "Take transmit from <short name>?" from txState,
//                       then tx.take with what was shown (ruling 8.7)
//   confirm.request     takeTransmit: the same question from its holder;
//                       sharedSetting / panMove: ConfirmChangeDialog;
//                       takeReceiver / takeSlice: TakeReceiverDialog;
//                       each answers confirm.proceed or confirm.cancel
//   notice              a NoticeCard on the band, Take it back when offered;
//                       controlTaken too (take-over parity): its Take it
//                       back is shown off, with the reason, when this Core
//                       cannot run it (controlTakeBackOff)
//   session.held        the Core is full: ReplaceDeviceDialog, answered
//                       with session.takeover (Task 78 item 7, G-53)
//   slice access        a refused listen, stop listening, take control or
//                       release, and a change held back on a slice this
//                       window only listens to, are refusals
//   markers             foreignMarkers() for each panadapter
//
// Nothing here keys the radio: a take never keys (the link document,
// section 18.9); the operator presses MOX after it.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-30: fix wave GUI-I1 and GUI-M6: dialogs close with their
//               session and are stamped with it; a card's Take it back is
//               shown off once its session ends. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
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
//   2026-09-29: slice control plan Task 10: questionDialog(), shared with
//               the hosting desktop's HostingSliceActions. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30: take-over parity: controlTaken is a card with Take it
//               back, shown off with a reason on an older Core. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30: desktop listening lane review: foreignMarkers() reads the
//               access entries, naming the hosting desktop for a slice the
//               station device holds. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-09-30: TX badge take fix round 1: lastTakeCommandId(), the
//               tx.take the window's own take question sent. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "gui/SpectrumWidget.h"

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QVector>

#include <functional>

class QDialog;
class QWidget;

namespace NereusSDR {

class NoticeCard;
class RemoteDevicesState;
class SliceAccessMirror;
struct RemotePrompt;
struct SessionPrompt;
class StationClient;

class MultiDeviceController : public QObject {
    Q_OBJECT

public:
    MultiDeviceController(StationClient* client, QWidget* dialogParent,
                          QObject* parent = nullptr);
    ~MultiDeviceController() override;

    /// Where notice cards go (the band: the active panadapter). Cards move
    /// with it.
    void setNoticeHost(QWidget* host);
    QWidget* noticeHost() const { return m_noticeHost; }
    /// Re-place the cards (the host was resized).
    void layoutNoticeCards();

    /// The operator asked to take transmit (the pan's TAKE TX pill, the TX
    /// applet's button). Asks first; nothing when transmit is not held
    /// elsewhere or this window cannot take it.
    void askTakeTransmit();

    /// Other devices' slices, as a panadapter draws them. A marker names
    /// no device for a slice the station device holds and for one nobody
    /// holds; `access` (null when the Core sends none) decides. A
    /// controller of "station" names the desktop that hosts the Core (its
    /// connectedDevices entry with hostsCore), or the Core itself on a Core
    /// no desktop hosts; otherwise nobody controls it.
    static QVector<SpectrumWidget::ForeignSliceMarker> foreignMarkers(
        const RemoteDevicesState& devices, const SliceAccessMirror* access = nullptr);

    /// The dialog for a confirm.request question: TakeTransmitDialog,
    /// TakeReceiverDialog (takeReceiver, takeSlice) or ConfirmChangeDialog.
    /// `choice`, when given, is set to what the answer's choice is read
    /// from when the dialog is accepted (-1 for none). A remote window and
    /// the hosting desktop ask with the same dialogs.
    static QDialog* questionDialog(const SessionPrompt& prompt, QWidget* parent,
                                   std::function<qint64()>* choice);
    /// Notice cards stacked from the foot of `host` upwards, newest at the
    /// foot (a remote window's and the hosting desktop's alike).
    static void stackNoticeCards(QWidget* host, QList<NoticeCard*> cards);

    /// The dialog open now (a question, or the window's own take question),
    /// or null.
    QDialog* openDialog() const { return m_dialog.data(); }
    /// The tx.take the window's own take question sent when it was last
    /// accepted (0: none, or the request could not be sent).
    quint32 lastTakeCommandId() const { return m_lastTakeCommandId; }
    QList<NoticeCard*> noticeCards() const;

    /// Take-over parity: why a notice's Take it back is shown off, or empty
    /// (it is offered, or the notice is not controlTaken). `available` is
    /// StationClient::controlTakeBackAvailable().
    static QString controlTakeBackOff(const RemotePrompt& notice, bool available);

signals:
    /// A refusal to show the operator (a take, an answer, Take it back; a
    /// slice access verb, a change held back on a listened slice).
    void refusal(const QString& reason);
    /// tx.take was accepted: this window holds transmit.
    void transmitTaken();
    /// The markers changed.
    void markersChanged();

private:
    void onQuestionChanged();
    void onHeldChanged();
    void onNoticesChanged();
    void onCommandFinished(const QByteArray& verb, quint32 commandId, bool accepted,
                           const QString& reason, bool awaitingConfirmation);
    void showDialog(QDialog* dialog);
    void closeDialogQuietly();
    // Fix wave GUI-I1 / GUI-M6: the session's epoch (0 without a client),
    // the session ending, and a stale card's Take it back shown off.
    quint32 currentEpoch() const;
    void onSessionEnded();
    void refreshCardsForSession();
    static QString sessionEndedTakeBackReason();

    QPointer<StationClient> m_client;
    QPointer<QWidget> m_dialogParent;
    QPointer<QWidget> m_noticeHost;
    QPointer<QDialog> m_dialog;
    /// The question the open dialog answers (0 for the window's own ask).
    qint64 m_dialogQuestionId = 0;
    quint32 m_lastTakeCommandId = 0;
    /// The open dialog is the fifth-device choice.
    bool m_dialogIsHeld = false;
    QHash<qint64, QPointer<NoticeCard>> m_cards;
    /// The session each card's notice came in (fix wave GUI-M6).
    QHash<qint64, quint32> m_cardEpochs;
    /// The session the open dialog was asked in (fix wave GUI-I1).
    quint32 m_dialogEpoch = 0;
    /// The session sessionEnded last ended (fix wave GUI-M6).
    quint32 m_endedEpoch = 0;
};

} // namespace NereusSDR
