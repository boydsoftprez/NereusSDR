#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// src/core/session/SliceMarker.h  (NereusSDR)
// =================================================================
//
// What another device sees of a slice (iPhone app plan Task 73, R-IOS-02;
// the several-devices design, docs/architecture/2026-09-24-several-devices-
// on-one-core-design.md, section 5.4, ruling 5.4; D46).
//
// Each device receives its own slices as `slice:<id>` objects and every
// other slice as a read-only marker: class SliceMarker, key
// `marker:<sliceId>`, one per slice, sent to every view but its owner's (for
// a held slice, the device it is held for) and never to an older window.
// Every property goes from the Core to the client:
//
//   sliceId         i64, in object.create only; the letter is 'A' + id
//   ownerDeviceId   the owner's id, or the id of the device it is held for
//   ownerName       the owner's name, numbered as `devices` numbers it
//   ownerShortName  the owner's short name, numbered the same way
//   ownerKind       phone, tablet, computer or station
//   ownerAway       the owner is in its 180 s, or the slice is held for it
//   frequency, dspMode, filterLow, filterHigh, band, streamIndex, psPaused
//                   as the slice's own
//   txSlice         the transmit slice while its owner holds transmit
//                   (ruling 5.4a, Task 77: SliceModel::txSliceMarked)
//
// A marker's colour is its letter's; no colour goes on the wire.
//
// SliceMarkerSet keeps one marker per slice of a RadioModel, watched in the
// Core's StateMirror under its key, and says when one is made or goes. The
// owner fields come from a resolver the Core's session server gives it
// (whose slice it is, the numbered names, whether the owner is away);
// refreshOwners() reads them again for every marker.
//
// Single thread: RadioModel's.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), iPhone app plan Task 73 (R-IOS-02), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-26: iPhone app plan Task 77 (R-IOS-02, R-IOS-03, R-IOS-13):
//               txSlice by ruling 5.4a. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
// =================================================================

#include "core/WdspTypes.h"
#include "core/session/MirrorSchema.h"
#include "models/Band.h"

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>

#include <functional>

namespace NereusSDR {

class RadioModel;
class SliceModel;
class StateMirror;

class SliceMarker : public QObject {
    Q_OBJECT
    Q_PROPERTY(int sliceId READ sliceId CONSTANT)
    Q_PROPERTY(QString ownerDeviceId READ ownerDeviceId NOTIFY ownerChanged)
    Q_PROPERTY(QString ownerName READ ownerName NOTIFY ownerChanged)
    Q_PROPERTY(QString ownerShortName READ ownerShortName NOTIFY ownerChanged)
    Q_PROPERTY(QString ownerKind READ ownerKind NOTIFY ownerChanged)
    Q_PROPERTY(bool ownerAway READ ownerAway NOTIFY ownerChanged)
    Q_PROPERTY(double frequency READ frequency NOTIFY frequencyChanged)
    Q_PROPERTY(NereusSDR::DSPMode dspMode READ dspMode NOTIFY dspModeChanged)
    Q_PROPERTY(int filterLow READ filterLow NOTIFY filterChanged)
    Q_PROPERTY(int filterHigh READ filterHigh NOTIFY filterChanged)
    Q_PROPERTY(bool txSlice READ txSlice NOTIFY txSliceChanged)
    Q_PROPERTY(NereusSDR::Band band READ band NOTIFY bandChanged)
    Q_PROPERTY(int streamIndex READ streamIndex NOTIFY streamIndexChanged)
    Q_PROPERTY(bool psPaused READ psPaused NOTIFY psPausedChanged)

public:
    /// Whose the slice is, as the wire names it.
    struct Owner {
        QString deviceId;
        QString name;
        QString shortName;
        QString kind;
        bool away = false;

        bool operator==(const Owner& other) const
        {
            return deviceId == other.deviceId && name == other.name
                && shortName == other.shortName && kind == other.kind && away == other.away;
        }
        bool operator!=(const Owner& other) const { return !(*this == other); }
    };

    explicit SliceMarker(SliceModel* slice, QObject* parent = nullptr);

    int sliceId() const { return m_sliceId; }
    QString ownerDeviceId() const { return m_owner.deviceId; }
    QString ownerName() const { return m_owner.name; }
    QString ownerShortName() const { return m_owner.shortName; }
    QString ownerKind() const { return m_owner.kind; }
    bool ownerAway() const { return m_owner.away; }
    double frequency() const;
    DSPMode dspMode() const;
    int filterLow() const;
    int filterHigh() const;
    bool txSlice() const;
    Band band() const;
    int streamIndex() const;
    bool psPaused() const;

    Owner owner() const { return m_owner; }
    /// ownerChanged when anything in it differs.
    void setOwner(const Owner& owner);

signals:
    void ownerChanged();
    void frequencyChanged();
    void dspModeChanged();
    void filterChanged();
    void txSliceChanged();
    void bandChanged();
    void streamIndexChanged();
    void psPausedChanged();

private:
    QPointer<SliceModel> m_slice;
    int m_sliceId = -1;
    Owner m_owner;
};

class SliceMarkerSet : public QObject {
    Q_OBJECT

public:
    /// The owner fields for slice `sliceId`, as the wire names them now.
    using OwnerResolver = std::function<SliceMarker::Owner(int sliceId)>;

    SliceMarkerSet(RadioModel* radio, StateMirror* mirror, OwnerResolver resolver,
                   QObject* parent = nullptr);
    ~SliceMarkerSet() override;

    /// "marker:<sliceId>".
    static QByteArray keyFor(int sliceId);
    /// The slice id of a "marker:<id>" key, or -1.
    static int sliceIdOf(const QByteArray& key);

    /// A marker for every slice the model holds that has none yet. Called
    /// when the Core's mirror is first built, as ObjectRegistry backfills.
    void backfill();
    /// Reads every marker's owner fields again.
    void refreshOwners();

    SliceMarker* marker(int sliceId) const;

signals:
    /// A marker was made and is watched; `snapshot` is its full property
    /// set.
    void markerCreated(const QByteArray& key, const QByteArray& className,
                       const QList<MirrorUpdate>& snapshot);
    /// A marker went with its slice; it is no longer watched.
    void markerDestroyed(const QByteArray& key, const QByteArray& className);

private:
    void createFor(int sliceId);
    void destroyFor(int sliceId);

    QPointer<RadioModel> m_radio;
    QPointer<StateMirror> m_mirror;
    OwnerResolver m_resolver;
    QHash<int, SliceMarker*> m_markers;
};

} // namespace NereusSDR
