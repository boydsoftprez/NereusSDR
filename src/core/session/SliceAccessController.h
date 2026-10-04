#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// src/core/session/SliceAccessController.h  (NereusSDR)
// =================================================================
//
// Listen in, stop listening, take control and release (slice control and
// shared listening plan Task 4; the design, docs/architecture/2026-09-28-
// slice-control-and-listening-design.md, "Approved behavior"; G-118).
//
// One place holds the checks and the change for each of these, so a
// remote device's command (slice.listen, slice.stopListening,
// slice.takeControl, slice.release, and setActiveSliceById on a slice it
// listens to) and the hosting desktop's own window (Task 10) run the same
// code. Each call names the slice by its SliceRef (id and incarnation), so
// a command never reaches a letter made again after a close, and take and
// release also carry the control revision the device saw, so exactly one
// of two devices that saw the same controller acts.
//
//   listen        joins the slice as a listener. Nothing is allocated (no
//                 slice, receiver stream or DDC); it works at full
//                 capacity. Already joined is accepted and changes
//                 nothing.
//   stopListening leaves it. Refused from its controller (ruling Q5: the
//                 controller uses Release). The device's receive choice
//                 moves to its next joined slice. A slice left with nobody
//                 on it closes, the Core's last included (Task 7); while it
//                 transmits the close waits and runs once the radio is in
//                 receive, if still nobody is on it.
//   takeControl   makes the device its controller, in one mark change: no
//                 slice is removed or made, its DSP channel and audio
//                 sources stay. The former controller stays a listener and
//                 is told (controlTaken). Refused while the slice
//                 transmits (checked here, at the change), and when its
//                 controller cannot stay on as a listener (ruling Q7). The
//                 former controller's transmit selection of the slice is
//                 cleared first (ruling Q8).
//   release       the controller only: control is cleared and it leaves.
//                 The slice stays for its other listeners, with no
//                 controller (ruling Q9: nobody adopts it; Take control
//                 does), or closes when nobody else is on it (the Core's
//                 last included, Task 7). Refused while the slice
//                 transmits, before anything closes or changes. The Core's other close paths
//                 act as this from a controller others listen with
//                 (ruling Q6).
//   selectRx      the device's active receive slice, any joined slice
//                 (RadioModel::setActiveRxFor).
//   setListenLevel
//                 the level (0..1) and mute a listener hears the slice at
//                 in its own mix (slice control plan Task 6). Each listener
//                 has its own; it starts at the slice's AF level when the
//                 device joins, and for a former controller when control
//                 passes from it (ruling Q4). Refused for a stale
//                 incarnation and for a slice the device does not hear.
//                 The station device's level plays on the Core's own output
//                 for a slice it listens to and another device controls
//                 (AudioEngine::setLocalListen); a session's goes to its
//                 owner mix (DaemonMediaController).
//
// What only the Core's session server knows (who is transmitting, which
// device can stay on as a listener, the transmit selection, closing a
// slice) comes in through Hooks.
//
// Single thread: RadioModel's.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-28: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), slice control and shared listening plan Task 4,
//               with AI-assisted implementation via Anthropic Claude Code.
//   2026-09-28: fix wave for the Tasks 1-4 review by J.J. Boyd (KG4VCF):
//               a take clears the transmit selection of the slice for any
//               holder, and a release that keeps the Core's last slice is
//               refused while it transmits. AI-assisted via Anthropic
//               Claude Code.
//   2026-09-29: slice control plan Task 6 by J.J. Boyd (KG4VCF): each
//               listener's own level and mute (setListenLevel, seeded from
//               the slice's AF, ruling Q4), and the station device's local
//               listening. AI-assisted via Anthropic Claude Code.
//   2026-09-29: slice control fix wave (whole-branch review, Critical 1)
//               by J.J. Boyd (KG4VCF): a release is refused while the
//               slice transmits before any close; the comments name the
//               last slice's close (Task 7). AI-assisted via Anthropic
//               Claude Code.
//   2026-09-30: core-slice take-over: Hooks::cannotHandOff is also given
//               the taker. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
//   2026-09-30: JJ's wider ruling: Hooks::staysListening. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30: TX badge take: takeWhileTransmittingWords, so a flag shows
//               the on-air refusal in the Core's own words. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/NereusCoreExport.h"
#include "core/SliceOwnership.h"

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>

#include <functional>

namespace NereusSDR {

class RadioModel;

class NEREUS_CORE_EXPORT SliceAccessController : public QObject {
    Q_OBJECT

public:
    struct Result {
        bool accepted = false;
        /// Plain words when refused.
        QString reason;
        /// The slice's control revision afterwards (listen, takeControl).
        quint64 controlRevision = 0;
        /// The object keys the change reached.
        QList<QByteArray> affected;
    };

    struct Hooks {
        /// Whether the slice is transmitting now: the transmit slice of a
        /// holder on the air, or the one the station freeze holds.
        std::function<bool(int sliceId)> transmitting;
        /// Why control of the slice cannot pass from `controller` to
        /// `taker` (it cannot stay on as a listener: an older window, a
        /// device that is away, the Core's own position for a taker that
        /// cannot take it), or empty when it can.
        std::function<QString(const QByteArray& controller, const QByteArray& taker,
                              int sliceId)>
            cannotHandOff;
        /// Ruling Q8: clears `former`'s transmit selection of the slice
        /// before control passes from it (`former` is empty for a slice
        /// with no controller), and that of any device holding transmit
        /// with the flag on the slice.
        std::function<void(const QByteArray& former, int sliceId)> clearTransmitSelection;
        /// JJ's wider ruling (2026-09-30): whether `former` stays joined as
        /// a listener after control is taken from it; one that cannot (a
        /// session without the feature) leaves the slice. Unset: it stays.
        std::function<bool(const QByteArray& former)> staysListening;
        /// Ruling Q8: `taker` took the slice; its transmit binding does not
        /// pick the slice up by itself.
        std::function<void(const QByteArray& taker, int sliceId)> tookControl;
        /// Closes a slice: one nobody is on whatever the count (slice
        /// control plan Task 7), or its controller's own close; false when
        /// it stays (the Core's last slice while its controller is still
        /// on it).
        std::function<bool(int sliceId)> close;
    };

    SliceAccessController(RadioModel* radio, Hooks hooks, QObject* parent = nullptr);

    Result listen(const QByteArray& device, SliceOwnership::SliceRef ref);
    Result stopListening(const QByteArray& device, SliceOwnership::SliceRef ref);
    Result takeControl(const QByteArray& device, SliceOwnership::SliceRef ref,
                       quint64 expectedRevision);
    Result release(const QByteArray& device, SliceOwnership::SliceRef ref,
                   quint64 expectedRevision);
    Result selectRx(const QByteArray& device, int sliceId);

    /// A listener's own level for a slice: 0..1 and its mute.
    struct ListenLevel {
        double level = 1.0;
        bool muted = false;
    };
    /// Slice control plan Task 6: the level and mute `device` hears the
    /// slice at as a listener. Refused for a stale incarnation and for a
    /// slice the device does not hear; `level` is clamped to 0..1.
    Result setListenLevel(const QByteArray& device, SliceOwnership::SliceRef ref, double level,
                          bool muted);
    /// The level `device` hears `sliceId` at as a listener: the one it set,
    /// else the slice's AF level when it joined or control passed from it
    /// (ruling Q4). The slice's AF level, unmuted, for one not recorded.
    ListenLevel listenLevel(const QByteArray& device, int sliceId) const;

    /// Whether a close of `sliceId` from `device` is a release (ruling Q6):
    /// it controls the slice and another device is joined to it.
    bool closeIsRelease(const QByteArray& device, int sliceId) const;

    /// The letter of `sliceId`, as the words name it.
    static QString letterOf(int sliceId);
    /// Why takeControl refuses while `sliceId` transmits, in its words (a
    /// window's TX badge holds with them while the slice is on the air).
    static QString takeWhileTransmittingWords(int sliceId);

signals:
    /// `byDevice` took control of the slice from `fromDevice`, which is
    /// still listening.
    void controlTaken(int sliceId, const QByteArray& fromDevice, const QByteArray& byDevice);
    /// `device`'s listen level or mute for the slice changed (set, seeded
    /// or dropped).
    void listenLevelChanged(int sliceId, const QByteArray& device);

private:
    SliceOwnership* ownership() const;
    static Result refused(const QString& reason);
    static QList<QByteArray> keysOf(int sliceId);
    /// Closes the slice when nobody is on it any more.
    void closeIfNobodyIsOn(int sliceId);
    /// The slice's AF level, 0..1.
    double afLevelOf(int sliceId) const;
    /// Seeds joined listeners, drops ones that left or a closed slice's.
    void reconcileListenLevels(int sliceId);
    /// Ruling Q4: control passed from `former`, which still listens.
    void reseedFormerController(int sliceId, const QByteArray& former);
    /// The station device's local listening of the slice.
    void publishLocalListen(int sliceId);

    struct StoredLevel {
        quint64 incarnation = 0;
        ListenLevel value;
    };

    QPointer<RadioModel> m_radio;
    Hooks m_hooks;
    /// Per slice, per listening device.
    QHash<int, QHash<QByteArray, StoredLevel>> m_listenLevels;
};

} // namespace NereusSDR
