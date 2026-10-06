#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// src/core/session/SliceAccessMirror.h  (NereusSDR)
// =================================================================
//
// A remote window's copy of who controls and who listens to each slice on
// the Core (slice control and shared listening plan Task 5). StationClient
// feeds it the `access:<id>` objects a Core sends a window that declared
// sliceAccess with sessionHolder (the link document, sections 7.1 and 7.5):
//
//   0 sliceId            i64
//   1 incarnation        i64, the slice's life on the Core
//   2 controllerDeviceId utf8, empty while nobody controls it; "station"
//                        for the Core's own position
//   3 controlRevision    i64, +1 on every change of controller
//   4 listenerDeviceIds  utf8, a JSON array of every joined device's id
//   5 activeRxDeviceIds  utf8, a JSON array of the devices whose active
//                        receive slice this is
//   6 txSelected         bool
//   7 onAir              bool
//
// It also marks each slice this window holds but does not control as a
// read-only listener (SliceModel::setReadOnlyListener), with the Core's own
// listener words, so the window's setters hold a change back instead of
// sending one the Core refuses. Nothing here is ever written back; the
// verbs go out through StationClient.
//
// Single thread: the StationClient's (the window's main thread).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-28: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), slice control and shared listening plan Task 5,
//               with AI-assisted implementation via Anthropic Claude Code.
//   2026-09-29: slice control plan Task 17: listenerReason names the
//               controller as the Core does. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-30: core-slice take-over: coreSliceTakeable(), the Core at
//               sliceAccessVersion 3 lets its own slice be taken. J.J.
//               Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
// =================================================================

#include "core/NereusCoreExport.h"
#include "core/session/MirrorSchema.h"

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>

#include <optional>

namespace NereusSDR {

class RadioModel;
class RemoteDevicesState;

class NEREUS_CORE_EXPORT SliceAccessMirror : public QObject {
    Q_OBJECT

public:
    /// One `access:<id>` object, as the Core last sent it.
    struct Entry {
        int sliceId;
        quint64 incarnation;
        QString controllerDeviceId;
        quint64 controlRevision;
        QStringList listeners;
        QStringList activeRx;
        bool txSelected;
        bool onAir;
    };

    /// `radio` is the window's model, whose slices are marked read-only;
    /// `devices` names the controller in the listener words. Either may be
    /// null (no marking, or "another device" for every controller but the
    /// station device).
    SliceAccessMirror(RadioModel* radio, RemoteDevicesState* devices,
                      QObject* parent = nullptr);

    /// Whether `objectKey` is an "access:<id>" key.
    static bool holdsKey(const QByteArray& objectKey);
    /// The slice id of an "access:<id>" key, or -1.
    static int sliceIdOf(const QByteArray& objectKey);

    /// An object.create or a delta for a key holdsKey() accepts.
    void applyObject(const QByteArray& objectKey, const QList<MirrorUpdate>& updates);
    /// An object.destroy.
    void destroyObject(const QByteArray& objectKey);
    /// The session ended: every entry was that session's. The slices keep
    /// their read-only marks until the next session says otherwise.
    void clear();

    /// This window's device id as the Core sends ids (StationClient sets it
    /// per session). Empty: no slice is taken to be controlled or listened
    /// to here.
    void setSelfDeviceId(const QString& id);
    QString selfDeviceId() const { return m_selfDeviceId; }

    /// Core-slice take-over (JJ, 2026-09-30): the Core sent
    /// sliceAccessVersion 3 or more, so a slice its own position controls
    /// (controllerDeviceId "station") may be taken like any other. Below
    /// 3 the Core refuses that take (StationClient::
    /// coreSliceTakeUnavailableReason). StationClient sets it from each
    /// capabilities message; false until then.
    void setCoreSliceTakeable(bool takeable);
    bool coreSliceTakeable() const { return m_coreSliceTakeable; }

    std::optional<Entry> entry(int sliceId) const;
    QList<Entry> entries() const { return m_entries.values(); }
    /// This window controls the slice.
    bool controlledHere(int sliceId) const;
    /// This window is joined to the slice (controlling it or not).
    bool listeningHere(int sliceId) const;
    /// The Core's words for a change this window may not make to the slice
    /// (SliceAccessPolicy's listener refusal, StationServer::
    /// listenerChangeReason).
    QString listenerReason(int sliceId) const;

    /// Marks every slice of the model again from the entries held now (a
    /// slice with no entry is not read-only).
    void refreshSlices();

signals:
    /// The entry for `sliceId` was made, changed or went.
    void changed(int sliceId);

private:
    void markSlice(int sliceId);
    static QStringList parseIds(const QString& json);

    QPointer<RadioModel> m_radio;
    QPointer<RemoteDevicesState> m_devices;
    QHash<int, Entry> m_entries;
    QString m_selfDeviceId;
    bool m_coreSliceTakeable = false;
};

} // namespace NereusSDR
