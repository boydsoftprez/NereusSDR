// =================================================================
// src/models/bandstack/BandStackController.h  (NereusSDR)
// =================================================================
//
// Connects the band stack's store to the Core's slices: performs recalls,
// tracks last-used state, runs dwell and expiry, and loads and saves the
// document under hardware/<mac>/bandstack/. Design:
// docs/architecture/2026-11-05-band-stack-design.md, sections 5 and 6.
//
// no-port-check: NereusSDR-original. The ported rules are in BandStackStore.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-05 - New for NereusSDR by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
// =================================================================

#pragma once

#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QtGlobal>

#include <functional>
#include <optional>

#include "core/NereusCoreExport.h"
#include "models/Band.h"
#include "models/bandstack/BandStackStore.h"

class QTimer;

namespace NereusSDR {

class RadioModel;
class SliceModel;

/// How a device's pan looks at a press. The Core has no pan object, so this
/// is only saved into a spot and used to decide what to publish.
struct BandStackView {
    std::optional<qint64> centerHz;
    std::optional<qint64> spanHz;
};

struct BandStackRecallRequest {
    enum Kind { Press, Step, GoTo } kind{Press};
    int sliceId{-1};
    Band band{Band::GEN};
    BandStackDirection direction{BandStackDirection::Next};
    QString entryId;
    BandStackView view;
};

struct BandStackRecallPlan {
    bool valid{false};
    qint64 frequencyHz{0};
    Band band{Band::GEN};
    QString refusal;
};

/// The last recall, published for the device controlling the slice to apply
/// to its own view. `spanHz` 0 means no saved zoom.
struct BandStackLastRecall {
    quint32 seq{0};
    int sliceId{-1};
    bool applyView{false};
    qint64 centerHz{0};
    qint64 spanHz{0};
};

/// One per radio, on the Core (Role::Local), on the main thread. Every method
/// returning bool gives false with `refusal` set (when non-null) and no change.
class NEREUS_CORE_EXPORT BandStackController : public QObject {
    Q_OBJECT

public:
    explicit BandStackController(RadioModel* model);
    ~BandStackController() override;

    /// The arguments of bandStack.press, bandStack.step or bandStack.goTo.
    /// Nullopt for any other verb or a malformed argument.
    static std::optional<BandStackRecallRequest> parseRecall(const QJsonObject& arguments,
                                                             const QString& verb);

    /// Where the recall would go. Changes nothing.
    BandStackRecallPlan planRecall(const BandStackRecallRequest& request) const;

    /// Runs design section 5.5. A retune the allocator would refuse leaves the
    /// slice and the store as they were.
    bool performRecall(const BandStackRecallRequest& request, QString* refusal);

    bool add(int sliceId, const BandStackView& view, QString* refusal);
    bool saveHere(int sliceId, const QString& entryId, const BandStackView& view, QString* refusal);
    bool edit(const QString& id, const std::optional<QString>& name, std::optional<bool> locked,
              QString* refusal);
    bool remove(const QString& id, QString* refusal);
    bool reorder(Band band, const QStringList& ids, QString* refusal);
    bool setReturn(Band band, BandStackReturnMode mode, const QString& entryId, QString* refusal);
    bool clear(std::optional<Band> band, QString* refusal);

    /// Replaces unlocked and seed entries with the current region's seeds.
    /// No slice moves.
    bool reseed(QString* refusal);

    bool options(std::optional<bool> dupes, std::optional<int> expiryMinutes,
                 std::optional<int> dwellSeconds, QString* refusal);

    /// Undoes the last expiry, for 5 seconds after it.
    bool restoreExpired(const QString& batchId, QString* refusal);

    const BandStackStore& store() const { return m_store; }

    /// The stack as clients see it: the store's document, without the place of
    /// a detached slice on the band it tuned into (design section 5.3).
    QJsonObject publishedDocument() const;

    BandStackLastRecall lastRecall() const { return m_lastRecall; }
    QString lastExpiryBatchId() const { return m_lastExpiryBatchId; }
    int lastExpiryCount() const { return m_lastExpiryCount; }
    QString regionName() const { return m_store.region(); }

    /// Writes a change still waiting for the coalescing timer.
    void flush();

    // Test seams. A test keeps its own clock and does not wait for the timers.
    void setClockForTest(std::function<qint64()> clock) { m_clockForTest = std::move(clock); }
    void runExpiryForTest() { runExpiry(); }
    void runDwellForTest(int sliceId) { runDwell(sliceId); }
    bool dwellArmedForTest(int sliceId) const;

signals:
    void changed();
    void reportRecall(int sliceId, const QString& text);

private:
    struct Resolution;
    struct RecalledBand {
        Band band{Band::GEN};
        Band gridBand{Band::GEN};
    };

    Resolution resolve(const BandStackRecallRequest& request) const;
    Band stackBandFor(const SliceModel* slice) const;
    QSet<QChar> slicesOnBand(Band band) const;
    BandStackSpot currentSpot(const SliceModel* slice, const BandStackView& view,
                              const BandStackEntry* existing) const;
    std::optional<BandStackSpot> lastSpot(const SliceModel* slice, Band band) const;
    QList<BandStackEntry> seedEntries() const;
    qint64 nowMs() const;

    void wireSlices();
    void onSliceTuned(SliceModel* slice);
    void armDwell(const SliceModel* slice);
    void stopDwell(int sliceId);
    void runDwell(int sliceId);
    void runExpiry();

    void loadDocument();
    void startFresh();
    void adoptSavedSpots(const SliceModel* slice);
    void followRegion();
    void markChanged();
    void writeDocument();
    QString documentKey() const;

    RadioModel* m_model{nullptr};
    BandStackStore m_store;
    QString m_mac;
    bool m_dirty{false};
    QTimer* m_saveTimer{nullptr};
    QTimer* m_expiryTimer{nullptr};
    std::function<qint64()> m_clockForTest;

    bool m_inRecall{false};
    QSet<const SliceModel*> m_wiredSlices;
    QHash<int, QTimer*> m_dwellTimers;
    // Each slice's last frequency, mode and filter per band, ahead of what its
    // per-band memory has been told.
    QMap<QChar, QMap<Band, BandStackSpot>> m_lastUsed;
    // A seed can sit outside the range of the band it is filed under (a GEN
    // row in 22m's range), so a slice recalled there is still on that band.
    QMap<QChar, RecalledBand> m_recalledBand;
    // The stack band each slice was last seen on, and the slices that got there
    // by anything but a recall, which stand detached from their place there: a
    // leave saves nothing (J.J.'s ruling, design section 5.3). Not in the document,
    // so every start begins with no slice detached.
    QMap<QChar, Band> m_onBand;
    QSet<QChar> m_detached;
    // Set when a fresh document started: slices that open later this session
    // bring their saved spots in too (design V8).
    bool m_adoptOpeningSlices{false};
    QSet<QChar> m_adopted;

    BandStackLastRecall m_lastRecall;
    QString m_lastExpiryBatchId;
    int m_lastExpiryCount{0};
    qint64 m_lastExpiryMs{0};
    QList<BandStackEntry> m_lastExpired;
    QMap<Band, QString> m_lastExpiredSpecific;
};

} // namespace NereusSDR
