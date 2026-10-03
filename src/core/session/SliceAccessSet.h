#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// src/core/session/SliceAccessSet.h  (NereusSDR)
// =================================================================
//
// Who controls and who listens to each slice, as a device that shares
// slices sees it (slice control and shared listening plan Task 4; the
// design, docs/architecture/2026-09-28-slice-control-and-listening-
// design.md, "Negotiate the new slice-access feature explicitly").
//
// One read-only SliceAccess object per live slice, key `access:<sliceId>`,
// sent only to a view that declared the hello feature `sliceAccess` 1 with
// sessionHolder (capability sliceAccessVersion 1, link section 6.3). Every
// property goes from the Core to the client:
//
//   0 sliceId            i64, in object.create only; the letter is 'A' + id
//   1 incarnation        i64, in object.create only: which slice of that
//                        letter this is (SliceOwnership::incarnation). The
//                        verbs carry it, so a command never reaches a
//                        letter made again after a close.
//   2 controllerDeviceId utf8, the controller's id as connectedDevices
//                        names it ("" for none; "station" for the Core's
//                        own operating position)
//   3 controlRevision    i64, +1 on every change of controller; take and
//                        release carry the one they saw
//   4 listenerDeviceIds  utf8, a JSON array of every joined device's id,
//                        the controller first, then in join order
//   5 activeRxDeviceIds  utf8, a JSON array of the devices whose active
//                        receive slice this is
//   6 txSelected         bool, the marker's txSlice (ruling 5.4a): the
//                        transmit slice while its controller holds transmit
//   7 onAir              bool, the slice is transmitting now
//
// SliceAccessSet keeps one object per slice of a RadioModel, watched in the
// Core's StateMirror under its key, and says when one is made or goes. Its
// fields come from a resolver the Core's session server gives it; the set
// reads them again whenever the slice's control, membership or TX mark
// changes, and the server calls refresh() when the transmitter does. The
// incarnation is read once, when the slice is made, so it stays readable
// while the slice is being removed.
//
// Single thread: RadioModel's.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-28: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), slice control and shared listening plan Task 4,
//               with AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include "core/session/MirrorSchema.h"

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>

#include <functional>

namespace NereusSDR {

class RadioModel;
class StateMirror;

class SliceAccess : public QObject {
    Q_OBJECT
    Q_PROPERTY(int sliceId READ sliceId CONSTANT)
    Q_PROPERTY(qint64 incarnation READ incarnation CONSTANT)
    Q_PROPERTY(QString controllerDeviceId READ controllerDeviceId NOTIFY controllerDeviceIdChanged)
    Q_PROPERTY(qint64 controlRevision READ controlRevision NOTIFY controlRevisionChanged)
    Q_PROPERTY(QString listenerDeviceIds READ listenerDeviceIds NOTIFY listenerDeviceIdsChanged)
    Q_PROPERTY(QString activeRxDeviceIds READ activeRxDeviceIds NOTIFY activeRxDeviceIdsChanged)
    Q_PROPERTY(bool txSelected READ txSelected NOTIFY txSelectedChanged)
    Q_PROPERTY(bool onAir READ onAir NOTIFY onAirChanged)

public:
    /// Everything but the id and the incarnation, as the wire names it.
    struct Fields {
        QString controllerDeviceId;
        qint64 controlRevision = 0;
        QString listenerDeviceIds = QStringLiteral("[]");
        QString activeRxDeviceIds = QStringLiteral("[]");
        bool txSelected = false;
        bool onAir = false;
    };

    SliceAccess(int sliceId, qint64 incarnation, QObject* parent = nullptr);

    int sliceId() const { return m_sliceId; }
    qint64 incarnation() const { return m_incarnation; }
    QString controllerDeviceId() const { return m_fields.controllerDeviceId; }
    qint64 controlRevision() const { return m_fields.controlRevision; }
    QString listenerDeviceIds() const { return m_fields.listenerDeviceIds; }
    QString activeRxDeviceIds() const { return m_fields.activeRxDeviceIds; }
    bool txSelected() const { return m_fields.txSelected; }
    bool onAir() const { return m_fields.onAir; }

    Fields fields() const { return m_fields; }
    /// Each property that differs is changed and announced.
    void setFields(const Fields& fields);

signals:
    void controllerDeviceIdChanged();
    void controlRevisionChanged();
    void listenerDeviceIdsChanged();
    void activeRxDeviceIdsChanged();
    void txSelectedChanged();
    void onAirChanged();

private:
    int m_sliceId = -1;
    qint64 m_incarnation = 0;
    Fields m_fields;
};

class SliceAccessSet : public QObject {
    Q_OBJECT

public:
    /// The fields of slice `sliceId`, as the wire names them now.
    using Resolver = std::function<SliceAccess::Fields(int sliceId)>;

    SliceAccessSet(RadioModel* radio, StateMirror* mirror, Resolver resolver,
                   QObject* parent = nullptr);
    ~SliceAccessSet() override;

    /// "access:<sliceId>".
    static QByteArray keyFor(int sliceId);
    /// The slice id of an "access:<id>" key, or -1.
    static int sliceIdOf(const QByteArray& key);

    /// An object for every slice the model holds that has none yet. Called
    /// when the Core's mirror is first built, as ObjectRegistry backfills.
    void backfill();
    /// Reads every object's fields again.
    void refresh();

    SliceAccess* access(int sliceId) const;

signals:
    /// An object was made and is watched; `snapshot` is its full property
    /// set.
    void accessCreated(const QByteArray& key, const QByteArray& className,
                       const QList<MirrorUpdate>& snapshot);
    /// An object went with its slice; it is no longer watched.
    void accessDestroyed(const QByteArray& key, const QByteArray& className);

private:
    void createFor(int sliceId);
    void destroyFor(int sliceId);
    void refreshOne(int sliceId);

    QPointer<RadioModel> m_radio;
    QPointer<StateMirror> m_mirror;
    Resolver m_resolver;
    QHash<int, SliceAccess*> m_objects;
    QHash<int, QMetaObject::Connection> m_txMarkWatches;
};

} // namespace NereusSDR
