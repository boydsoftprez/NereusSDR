#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// src/core/session/ReceiverPlanner.h  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 74 (R-IOS-02, R-IOS-30; the several-devices design,
// sections 6.1 to 6.4 and 7.3, rulings 6.2 to 6.9 and design ruling 6.5a).
//
// Up to four devices share the radio's receivers. This class says, before
// anything changes, what a pan move or a take would do to the other
// devices' slices, so the Core can ask first and name them:
//
//   - planWindowMove: a receiver's window moving to a new centre (the
//     anchor's C-Tune move, ruling 6.4, or its band change, ruling 6.5).
//     Each slice of another device the new window no longer covers either
//     `moves` (another window covers it, or a free receiver takes it) or
//     `closes` (none is free), simulated on a copy of the Core's placement
//     policy exactly as moveStreamWindowFor will apply it. The requester's
//     own slices outside the window are listed apart (today's C-Tune rule
//     refuses them; design ruling 6.5a turns a band change into a plain
//     retune).
//   - receiverChoices: the take chooser (section 6.4): one choice per
//     receiver in use, its slices and devices, and whether it may be taken
//     (ruling 6.8's on-air case once transmit has a holder; a receiver the
//     requester's own slice would be stranded on, design ruling 6.5a; a
//     receiver the requester's own panadapter already uses, for a new pan).
//   - sliceChoices: the slice chooser (ruling 6.9).
//
// It reads the Core's model and changes nothing. Single thread: the
// model's.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), iPhone app plan Task 74 (R-IOS-02, R-IOS-30),
//               with AI-assisted implementation via Anthropic Claude Code.
//   2026-09-29: slice control plan Task 9: Disturbed::listeners and
//               setShowListeners (listenerDeviceIds). J.J. Boyd (KG4VCF),
//               with AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include <QByteArray>
#include <QJsonArray>
#include <QList>
#include <QString>
#include <QStringList>

#include <functional>

#include "models/Band.h"

namespace NereusSDR {

class RadioModel;

class ReceiverPlanner {
public:
    /// One device as the Core names it (ConnectedDevicesFacade::describe,
    /// with the registry's state).
    struct DeviceInfo {
        bool known = false;
        QString wireId;
        QString name;
        QString shortName;
        QString kind;
        /// "listening", "transmitting" or "away".
        QString state = QStringLiteral("listening");
        qint64 lastActivitySeconds = 0;
    };
    using Describe = std::function<DeviceInfo(const QByteArray& deviceId)>;

    ReceiverPlanner(const RadioModel& model, Describe describe);

    /// Ruling 6.8 (D64): the transmit slice of a holder on the air, and the
    /// holder, so the receiver carrying it is listed not takeable with
    /// `why`. Unset (sliceId -1) while nobody is on the air.
    struct OnAirTransmit {
        int sliceId = -1;
        QByteArray holder;
        QString why;
    };
    void setOnAirTransmit(OnAirTransmit onAir) { m_onAir = std::move(onAir); }

    /// Slice control plan Task 9: whether the JSON below names each slice's
    /// listeners (`listenerDeviceIds`, controller first). Only a device
    /// that shares slices is sent them.
    void setShowListeners(bool on) { m_showListeners = on; }

    enum class Effect { Moves, Closes };
    struct Disturbed {
        int sliceId = -1;
        /// Whose it is (Mark::subject()).
        QByteArray device;
        Effect effect = Effect::Closes;
        /// Slice control plan Task 9: every device joined to the slice
        /// when planned, its controller first (SliceOwnership::listenersOf).
        QList<QByteArray> listeners;
    };

    struct WindowMove {
        bool valid = false;
        /// The requester's own slices the new window does not cover.
        QList<int> ownOutside;
        /// Other devices' slices it does not cover, in the order they are
        /// placed again.
        QList<Disturbed> disturbed;
    };
    /// `stream` moving to `centreHz`, as `requester`; `exemptSliceId` is
    /// the slice whose own retune moves it (it goes with the window).
    WindowMove planWindowMove(int stream, double centreHz, const QByteArray& requester,
                              int exemptSliceId) const;

    /// What needs a receiver (section 6.4's trigger).
    enum class Need { AddSlice, AddPan, Retune, PanMove };
    struct TakeRequest {
        Need need = Need::AddSlice;
        QByteArray requester;
        /// The new window's centre (the slice's new frequency, the pan's
        /// asked-for centre); unused for an add.
        double centreHz = 0.0;
        /// The requester's slices that go with the change (a retuned
        /// slice, the slices of a moving pan).
        QList<int> moving;
    };
    struct Choice {
        int choice = 0;
        int stream = -1;
        /// For the slice chooser: the slice taken.
        int sliceId = -1;
        bool takeable = true;
        QString why;
        /// Every other device's slice that closes (ruling 6.7).
        QList<int> closes;
    };
    /// One choice per receiver in use, lowest first.
    QList<Choice> receiverChoices(const TakeRequest& request) const;
    /// One choice per slice of another device (ruling 6.9).
    QList<Choice> sliceChoices(const QByteArray& requester) const;

    struct AddPlacement {
        bool sliceSpace = false;
        bool receiverFits = false;
        bool mayClose = true;
        QString reason;
        bool fits() const { return sliceSpace && receiverFits && mayClose; }
    };
    /// Predict the Add that follows an exact confirmed victim set, using
    /// the same allocator and owner+pan rule as RadioModel::addSliceImpl.
    AddPlacement planAddAfterClosing(const QByteArray& requester, const QString& panId,
                                     const QList<int>& closes) const;

    /// Predict whether every carried slice still fits the requested pan
    /// window after this exact receiver choice closes its other occupants.
    bool panMoveFitsAfterClosing(int stream, double centreHz, const QList<int>& moving,
                                 const QList<int>& closes) const;

    struct RestorePlacement {
        bool fits = false;
        QString reason;
    };
    /// Predict restoring the whole saved group after the shown victims close.
    /// A refusal must leave the saved Take-back claim and victims untouched.
    RestorePlacement planRestoreAfterClosing(const QList<double>& frequencies,
                                              const QList<int>& closes) const;

    /// Whether any receiver in use carries a slice of a device other than
    /// `requester`: with none, a take has nobody to take from.
    bool anotherDeviceHoldsAReceiver(const QByteArray& requester) const;
    /// The numbered names of the devices whose slices hold the radio's
    /// receivers, but `requester`, in the order they first appear.
    QStringList namesHoldingReceivers(const QByteArray& requester) const;

    // ---- JSON for confirm.request (the link document, section 7.3) ----

    QJsonArray affectedJson(const QList<Disturbed>& disturbed) const;
    QJsonArray receiverChoicesJson(const QList<Choice>& choices) const;
    QJsonArray sliceChoicesJson(const QList<Choice>& choices) const;
    /// [{sliceId, letter, frequencyHz, mode, band}] for these live slices.
    QJsonArray noticeSlicesJson(const QList<int>& sliceIds) const;

    // ---- Words ----

    /// "40 m", "20 m"; a band without metres by its own label.
    static QString bandWords(Band band);
    /// "A", "A and B", "A, B and C".
    static QString joinWords(const QStringList& words);
    static QString letterOf(int sliceId);
    static QString effectName(Effect effect);

private:
    bool windowCovers(int stream, double centreHz, double frequencyHz) const;
    QByteArray subjectOf(int sliceId) const;
    QJsonArray listenerIdsJson(const QList<QByteArray>& listeners) const;

    const RadioModel& m_model;
    Describe m_describe;
    OnAirTransmit m_onAir;
    bool m_showListeners = false;
};

} // namespace NereusSDR
