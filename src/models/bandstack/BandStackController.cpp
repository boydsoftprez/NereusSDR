// =================================================================
// src/models/bandstack/BandStackController.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. The ported rules are in BandStackStore.
//
// A recall goes through the same slice setters a band button uses and does
// not touch the I/Q, WDSP or audio path. See BandStackController.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-05 - New for NereusSDR by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
// =================================================================

#include "models/bandstack/BandStackController.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonValue>
#include <QLoggingCategory>
#include <QScopeGuard>
#include <QTimer>
#include <QUuid>

#include <limits>
#include <utility>

#include "core/AppSettings.h"
#include "core/LogCategories.h"
#include "core/SliceOwnership.h"
#include "core/safety/BandPlanGuard.h"
#include "models/BandDefaults.h"
#include "models/FilterPresetStore.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/bandstack/BandStackSeeds.h"

namespace NereusSDR {

namespace {

// Design section 6.2: saved at most once a second after a change.
constexpr int kSaveCoalesceMs = 1000;
// Design V5, with the April text's 60 second pass.
constexpr int kExpiryPassMs = 60000;
// Design section 6.3: an expiry can be undone for 5 seconds.
constexpr qint64 kExpiryUndoMs = 5000;
// The store's duplicate window: within 100 Hz is the same spot.
constexpr qint64 kSameSpotHz = 100;

const QString kNoEntry = QStringLiteral("That band stack entry no longer exists.");

void setRefusal(QString* refusal, const QString& text)
{
    if (refusal != nullptr) {
        *refusal = text;
    }
}

// SliceModel::modeFromName answers USB for a name it does not know, so a name
// counts only when it comes back the same.
std::optional<DSPMode> knownMode(const QString& name)
{
    const DSPMode mode = SliceModel::modeFromName(name);
    if (SliceModel::modeName(mode) != name) {
        return std::nullopt;
    }
    return mode;
}

safety::Region currentRegion()
{
    bool ok = false;
    const int region = AppSettings::instance()
                           .value(QStringLiteral("BandPlanRegion"),
                                  QString::number(static_cast<int>(safety::Region::UnitedStates)))
                           .toString()
                           .toInt(&ok);
    if (!ok || region < static_cast<int>(safety::Region::Australia)
        || region > static_cast<int>(safety::Region::Germany)) {
        qCWarning(lcApp) << "Band stack: the region setting is invalid, using United States";
        return safety::Region::UnitedStates;
    }
    return static_cast<safety::Region>(region);
}

// False when the key is there and is not an integer.
bool readOptionalHz(const QJsonObject& object, const QString& key, std::optional<qint64>* out)
{
    const QJsonValue value = object.value(key);
    if (value.isUndefined() || value.isNull()) {
        return true;
    }
    constexpr qint64 kNotInteger = std::numeric_limits<qint64>::min();
    const qint64 integer = value.toInteger(kNotInteger);
    if (!value.isDouble() || integer == kNotInteger) {
        return false;
    }
    *out = integer;
    return true;
}

std::optional<BandStackDirection> directionFromName(const QString& name)
{
    if (name == QLatin1String("next")) {
        return BandStackDirection::Next;
    }
    if (name == QLatin1String("previous")) {
        return BandStackDirection::Previous;
    }
    return std::nullopt;
}

} // namespace

// What a recall would do, worked out on a copy of the store, so a plan changes
// nothing and a refused recall leaves the real store as it was.
struct BandStackController::Resolution {
    BandStackRecallPlan plan;
    SliceModel* slice{nullptr};
    Band leftBand{Band::GEN};
    BandStackSpot leftSpot;
    QString mode;
    int filterLow{0};
    int filterHigh{0};
    QString entryId;
    bool hasEntry{false};
    QString report;
    BandStackStore after;
};

BandStackController::BandStackController(RadioModel* model)
    : QObject(model)
    , m_model(model)
{
    m_saveTimer = new QTimer(this);
    m_saveTimer->setSingleShot(true);
    m_saveTimer->setInterval(kSaveCoalesceMs);
    connect(m_saveTimer, &QTimer::timeout, this, [this] {
        writeDocument();
        // The value is in the settings store; this gets it to disk.
        m_model->requestSettingsSave();
    });

    m_expiryTimer = new QTimer(this);
    m_expiryTimer->setInterval(kExpiryPassMs);
    connect(m_expiryTimer, &QTimer::timeout, this, &BandStackController::runExpiry);
    m_expiryTimer->start();

    connect(m_model, &RadioModel::sliceAdded, this, [this](int) { wireSlices(); });
    connect(m_model, &RadioModel::bandPlanRegionChanged, this, &BandStackController::followRegion);
    connect(m_model, &RadioModel::currentRadioChanged, this,
            [this](const RadioInfo&) { loadDocument(); });
    // Putting a slice back where it was at connect is not tuning into a band.
    connect(m_model, &RadioModel::sliceStateRestored, this, [this](int sliceId) {
        const SliceModel* slice = m_model->sliceById(sliceId);
        if (slice == nullptr) {
            return;
        }
        m_onBand[slice->sliceLetter()] = stackBandFor(slice);
        if (m_detached.remove(slice->sliceLetter())) {
            emit changed();
        }
    });
    if (QCoreApplication::instance() != nullptr) {
        connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this,
                &BandStackController::flush);
    }

    wireSlices();
    loadDocument();
}

BandStackController::~BandStackController()
{
    writeDocument();
}

std::optional<BandStackRecallRequest> BandStackController::parseRecall(const QJsonObject& arguments,
                                                                       const QString& verb)
{
    BandStackRecallRequest request;
    if (verb == QLatin1String("bandStack.press")) {
        request.kind = BandStackRecallRequest::Press;
    } else if (verb == QLatin1String("bandStack.step")) {
        request.kind = BandStackRecallRequest::Step;
    } else if (verb == QLatin1String("bandStack.goTo")) {
        request.kind = BandStackRecallRequest::GoTo;
    } else {
        return std::nullopt;
    }

    std::optional<qint64> sliceId;
    if (!readOptionalHz(arguments, QStringLiteral("sliceId"), &sliceId) || !sliceId.has_value()
        || *sliceId < 0 || *sliceId > std::numeric_limits<int>::max()) {
        return std::nullopt;
    }
    request.sliceId = static_cast<int>(*sliceId);

    if (!readOptionalHz(arguments, QStringLiteral("viewCenterHz"), &request.view.centerHz)
        || !readOptionalHz(arguments, QStringLiteral("viewSpanHz"), &request.view.spanHz)) {
        return std::nullopt;
    }

    if (request.kind == BandStackRecallRequest::GoTo) {
        const QJsonValue entryId = arguments.value(QStringLiteral("entryId"));
        if (!entryId.isString() || entryId.toString().isEmpty()) {
            return std::nullopt;
        }
        request.entryId = entryId.toString();
        return request;
    }

    if (request.kind == BandStackRecallRequest::Press) {
        const std::optional<Band> band =
            BandStackStore::bandFromKey(arguments.value(QStringLiteral("band")).toString());
        if (!band.has_value()) {
            return std::nullopt;
        }
        request.band = *band;
    }

    const std::optional<BandStackDirection> direction =
        directionFromName(arguments.value(QStringLiteral("direction")).toString());
    if (!direction.has_value()) {
        return std::nullopt;
    }
    request.direction = *direction;
    return request;
}

// The slice's band for the stack: the band its last recall put it on while it
// stays in that frequency's range, then a place holding this very frequency
// (the same thing after a restart), then the band its frequency falls in.
Band BandStackController::stackBandFor(const SliceModel* slice) const
{
    const double hz = slice->frequency();
    const Band grid = BandDefaults::bandGridBandFromFrequency(hz);
    const QChar letter = slice->sliceLetter();

    const auto recalled = m_recalledBand.constFind(letter);
    if (recalled != m_recalledBand.constEnd() && recalled->gridBand == grid) {
        return recalled->band;
    }

    const auto holdsThisFrequency = [&](Band band) {
        const BandStackEntry* entry = m_store.entry(m_store.place(letter, band));
        return entry != nullptr
               && qAbs(entry->frequencyHz - qRound64(hz)) < kSameSpotHz;
    };
    if (holdsThisFrequency(grid)) {
        return grid;
    }
    for (int i = 0; i < static_cast<int>(Band::Count); ++i) {
        const Band band = static_cast<Band>(i);
        if (holdsThisFrequency(band)) {
            return band;
        }
    }
    return grid;
}

QSet<QChar> BandStackController::slicesOnBand(Band band) const
{
    QSet<QChar> letters;
    for (const SliceModel* slice : m_model->slices()) {
        // A detached slice is on the band but not on its place there (design 5.3).
        if (slice != nullptr && stackBandFor(slice) == band
            && !m_detached.contains(slice->sliceLetter())) {
            letters.insert(slice->sliceLetter());
        }
    }
    return letters;
}

// Where a view is missing, the entry keeps its stored values (design 6.3).
BandStackSpot BandStackController::currentSpot(const SliceModel* slice, const BandStackView& view,
                                               const BandStackEntry* existing) const
{
    const qint64 hz = qRound64(slice->frequency());
    BandStackSpot spot;
    spot.frequencyHz = hz;
    spot.mode = SliceModel::modeName(slice->dspMode());
    spot.filterLowHz = slice->filterLow();
    spot.filterHighHz = slice->filterHigh();
    spot.centerHz = view.centerHz.value_or(existing != nullptr ? existing->centerHz : hz);
    spot.spanHz = view.spanHz.value_or(existing != nullptr ? existing->spanHz : 0);
    return spot;
}

std::optional<BandStackSpot> BandStackController::lastSpot(const SliceModel* slice, Band band) const
{
    const auto bands = m_lastUsed.constFind(slice->sliceLetter());
    if (bands != m_lastUsed.constEnd()) {
        const auto spot = bands->constFind(band);
        if (spot != bands->constEnd()) {
            return *spot;
        }
    }
    const std::optional<SliceModel::SavedBandSpot> saved = slice->savedBandSpot(band);
    if (!saved.has_value()) {
        return std::nullopt;
    }
    BandStackSpot spot;
    spot.frequencyHz = qRound64(saved->frequencyHz);
    spot.mode = SliceModel::modeName(saved->mode);
    spot.filterLowHz = saved->filterLow;
    spot.filterHighHz = saved->filterHigh;
    spot.centerHz = spot.frequencyHz;
    return spot;
}

BandStackController::Resolution
BandStackController::resolve(const BandStackRecallRequest& request) const
{
    Resolution r;
    SliceModel* slice = m_model->sliceById(request.sliceId);
    if (slice == nullptr) {
        r.plan.refusal = QStringLiteral("That slice is not open.");
        return r;
    }
    r.slice = slice;
    const QChar letter = slice->sliceLetter();
    const Band current = stackBandFor(slice);

    Band band = request.band;
    if (request.kind == BandStackRecallRequest::Step) {
        band = current;
    } else if (request.kind == BandStackRecallRequest::GoTo) {
        const BandStackEntry* entry = m_store.entry(request.entryId);
        if (entry == nullptr) {
            r.plan.refusal = kNoEntry;
            return r;
        }
        band = entry->band;
    }
    r.plan.band = band;

    // Design section 5.5 step 1. Who may press for a slice is the link's check.
    if (slice->locked()) {
        r.plan.refusal = QStringLiteral(
                             "Band %1 ignored: the slice is locked. Unlock it to change bands.")
                             .arg(bandLabel(band));
        return r;
    }
    if (m_model->txBoundSlice() == slice && m_model->isTransmitting()) {
        r.plan.refusal = QStringLiteral("Band %1 ignored: Slice %2 is transmitting.")
                             .arg(bandLabel(band), QString(letter));
        return r;
    }
    if (band == Band::XVTR) {
        // Design V12, in the words of RadioModel::onBandButtonClicked.
        r.plan.refusal = QStringLiteral("Band %1 ignored: transverter settings are not available")
                             .arg(bandLabel(band));
        return r;
    }

    // Save first, so a press that lands on the spot it just left (one entry,
    // wrapping) goes to what was just saved.
    BandStackStore after = m_store;
    const QString placeId = after.place(letter, current);
    const BandStackEntry* leaving = after.entry(placeId);
    r.leftBand = current;
    r.leftSpot = currentSpot(slice, request.view, leaving);
    // Going to your own place is going back to it, so nothing is left to save:
    // writing here would replace the spot asked for with where the slice wandered
    // to, and clear a seed's mark (design section 5.3).
    const bool goingBack =
        request.kind == BandStackRecallRequest::GoTo && request.entryId == placeId;
    // J.J.'s ruling, 2026-11-06: a slice that tuned into this band never came to
    // its place here, so leaving writes nothing into it (design section 5.3).
    const bool detached = m_detached.contains(letter);
    if (!goingBack && !detached) {
        after.saveOnLeave(letter, current, r.leftSpot, nowMs(), slicesOnBand(current));
    }

    BandStackTarget target;
    switch (request.kind) {
    case BandStackRecallRequest::Press:
        // Detached, a press of this band arrives at the place, not a step past it.
        target = detached && band == current
            ? after.planArrive(letter, band)
            : after.planPress(letter, current, band, request.direction);
        break;
    case BandStackRecallRequest::Step:
        target = after.planStep(letter, band, request.direction);
        break;
    case BandStackRecallRequest::GoTo:
        target = {BandStackTarget::Entry, request.entryId, band};
        break;
    }

    std::optional<BandStackSpot> last;
    if (target.kind == BandStackTarget::LastUsed) {
        last = lastSpot(slice, band);
        if (!last.has_value()) {
            target = after.planCurrent(letter, band);
        }
    }

    switch (target.kind) {
    case BandStackTarget::Entry: {
        const BandStackEntry* entry = after.entry(target.entryId);
        if (entry == nullptr) {
            r.plan.refusal = kNoEntry;
            return r;
        }
        if (!knownMode(entry->mode).has_value()) {
            r.plan.refusal = QStringLiteral("That band stack entry has an unknown mode.");
            return r;
        }
        r.plan.frequencyHz = entry->frequencyHz;
        r.mode = entry->mode;
        r.filterLow = entry->filterLowHz;
        r.filterHigh = entry->filterHighHz;
        r.entryId = entry->id;
        r.hasEntry = true;
        const QList<BandStackEntry> onBand = after.entries(band);
        qsizetype position = 0;
        while (position < onBand.size() && onBand.at(position).id != entry->id) {
            ++position;
        }
        r.report = QStringLiteral("%1: %2 %3, %4 of %5")
                       .arg(bandKeyName(band),
                            BandStackStore::displayFrequency(entry->frequencyHz), entry->mode)
                       .arg(position + 1)
                       .arg(onBand.size());
        const QString id = entry->id;
        after.setPlace(letter, band, id);
        after.markUsed(id, nowMs());
        break;
    }
    case BandStackTarget::LastUsed:
        r.plan.frequencyHz = last->frequencyHz;
        r.mode = last->mode;
        r.filterLow = last->filterLowHz;
        r.filterHigh = last->filterHighHz;
        r.report = QStringLiteral("%1: %2 %3")
                       .arg(bandKeyName(band), BandStackStore::displayFrequency(last->frequencyHz),
                            last->mode);
        // It is not an entry, so the slice has no place here (design 5.2).
        after.clearPlace(letter, band);
        break;
    case BandStackTarget::EmptyBand: {
        const std::optional<double> middle = BandDefaults::bandMiddleHz(band);
        if (!middle.has_value()) {
            r.plan.refusal = QStringLiteral("Band %1 ignored: it has no starting frequency")
                                 .arg(bandLabel(band));
            return r;
        }
        r.plan.frequencyHz = qRound64(*middle);
        // The band's own starting mode, as a first visit to a band takes. The
        // filter is whatever that mode's change brings.
        const BandSeed seed = BandDefaults::seedFor(band);
        if (seed.valid) {
            r.mode = SliceModel::modeName(seed.mode);
        }
        r.report = QStringLiteral("%1: empty stack, tuned to the middle of the band")
                       .arg(bandKeyName(band));
        break;
    }
    case BandStackTarget::None:
        r.plan.refusal = QStringLiteral("%1 has no band stack entries to step to.")
                             .arg(bandLabel(band));
        return r;
    }

    r.after = std::move(after);
    r.plan.valid = true;
    return r;
}

BandStackRecallPlan BandStackController::planRecall(const BandStackRecallRequest& request) const
{
    return resolve(request).plan;
}

bool BandStackController::performRecall(const BandStackRecallRequest& request, QString* refusal)
{
    Resolution r = resolve(request);
    if (!r.plan.valid) {
        setRefusal(refusal, r.plan.refusal);
        return false;
    }

    SliceModel* slice = r.slice;
    const double targetHz = static_cast<double>(r.plan.frequencyHz);
    const QString retuneRefusal = m_model->retuneRefusalFor(slice, targetHz);
    if (!retuneRefusal.isEmpty()) {
        setRefusal(refusal, retuneRefusal);
        return false;
    }

    m_inRecall = true;
    const auto endRecall = qScopeGuard([this] { m_inRecall = false; });
    stopDwell(slice->sliceIndex());

    // Design section 5.5 steps 2 to 4.
    const Band band = r.plan.band;
    m_model->saveBandMemory(slice, r.leftBand);
    if (band != r.leftBand) {
        // J.J.'s ruling, 2026-11-05: one tune step per slice on every band, so
        // a recall never restores one from per-band memory. Put back whatever
        // the restore did. Harmless once the restore stops touching the step.
        const int stepHz = slice->stepHz();
        slice->restoreFromSettings(band, false);
        slice->setStepHz(stepHz);
    }

    slice->setFrequency(targetHz);
    if (!r.mode.isEmpty()) {
        const std::optional<DSPMode> mode = knownMode(r.mode);
        if (mode.has_value()) {
            slice->setDspMode(*mode);
        }
    }
    if (r.filterLow != 0 || r.filterHigh != 0) {
        slice->setFilter(r.filterLow, r.filterHigh);
    }

    // The check above should have caught this; a rollback still saves nothing.
    if (qRound64(slice->frequency()) != r.plan.frequencyHz) {
        setRefusal(refusal, QStringLiteral("Slice %1 stayed on %2.")
                                .arg(QString(slice->sliceLetter()),
                                     BandStackStore::displayFrequency(qRound64(slice->frequency()))));
        return false;
    }
    m_model->saveBandMemory(slice, band);

    // Design section 5.5 step 6.
    const QChar letter = slice->sliceLetter();
    m_store = std::move(r.after);
    m_lastUsed[letter][r.leftBand] = r.leftSpot;
    m_lastUsed[letter][band] = currentSpot(slice, {}, nullptr);
    m_recalledBand[letter] = {band, BandDefaults::bandGridBandFromFrequency(targetHz)};
    m_onBand[letter] = band;
    m_detached.remove(letter);

    // Design section 5.5 step 5 is the device's to apply; here is what it needs.
    const BandStackEntry* entry = r.hasEntry ? m_store.entry(r.entryId) : nullptr;
    const QByteArray owner = m_model->sliceOwnership()->mark(slice->sliceIndex()).owner;
    const bool alone = !m_model->panHasSlicesFor(slice->panKey(), owner, slice);
    const bool onScreen =
        request.view.centerHz.has_value() && request.view.spanHz.has_value()
        && qAbs(r.plan.frequencyHz - *request.view.centerHz) <= *request.view.spanHz / 2;
    ++m_lastRecall.seq;
    m_lastRecall.sliceId = slice->sliceIndex();
    m_lastRecall.applyView = alone || !onScreen;
    // Design V14: the saved zoom and center are only for a pan the slice has to
    // itself. A shared pan follows as a band change does: the frequency, at the
    // span it has, so the other slice's view is not narrowed.
    const BandStackEntry* savedView = alone ? entry : nullptr;
    m_lastRecall.centerHz = savedView != nullptr && savedView->centerHz != 0 ? savedView->centerHz
                                                                             : r.plan.frequencyHz;
    m_lastRecall.spanHz = savedView != nullptr ? savedView->spanHz : 0;

    markChanged();
    emit reportRecall(slice->sliceIndex(), r.report);
    return true;
}

bool BandStackController::add(int sliceId, const BandStackView& view, QString* refusal)
{
    const SliceModel* slice = m_model->sliceById(sliceId);
    if (slice == nullptr) {
        setRefusal(refusal, QStringLiteral("That slice is not open."));
        return false;
    }
    const QString id = m_store.add(slice->sliceLetter(), stackBandFor(slice),
                                   currentSpot(slice, view, nullptr), nowMs(), refusal);
    if (id.isEmpty()) {
        return false;
    }
    // The new entry is the slice's place, and it is standing on it.
    m_detached.remove(slice->sliceLetter());
    markChanged();
    return true;
}

bool BandStackController::saveHere(int sliceId, const QString& entryId, const BandStackView& view,
                                   QString* refusal)
{
    const SliceModel* slice = m_model->sliceById(sliceId);
    if (slice == nullptr) {
        setRefusal(refusal, QStringLiteral("That slice is not open."));
        return false;
    }
    const BandStackEntry* entry = m_store.entry(entryId);
    if (entry == nullptr) {
        setRefusal(refusal, kNoEntry);
        return false;
    }
    if (entry->locked) {
        setRefusal(refusal, QStringLiteral("That entry is locked. Unlock it to save here."));
        return false;
    }
    if (entry->band != stackBandFor(slice)) {
        setRefusal(refusal, QStringLiteral("Slice %1 is not on %2.")
                                .arg(QString(slice->sliceLetter()), bandLabel(entry->band)));
        return false;
    }
    m_store.saveHere(slice->sliceLetter(), entryId, currentSpot(slice, view, entry), nowMs());
    // Same as Add: that entry is the slice's place now, and it is standing on it.
    m_detached.remove(slice->sliceLetter());
    markChanged();
    return true;
}

bool BandStackController::edit(const QString& id, const std::optional<QString>& name,
                               std::optional<bool> locked, QString* refusal)
{
    if (!m_store.edit(id, name, locked)) {
        setRefusal(refusal, kNoEntry);
        return false;
    }
    markChanged();
    return true;
}

bool BandStackController::remove(const QString& id, QString* refusal)
{
    if (!m_store.remove(id)) {
        setRefusal(refusal, kNoEntry);
        return false;
    }
    markChanged();
    return true;
}

bool BandStackController::reorder(Band band, const QStringList& ids, QString* refusal)
{
    if (!m_store.reorder(band, ids)) {
        setRefusal(refusal,
                   QStringLiteral("That list does not match the entries on %1.").arg(bandLabel(band)));
        return false;
    }
    markChanged();
    return true;
}

bool BandStackController::setReturn(Band band, BandStackReturnMode mode, const QString& entryId,
                                    QString* refusal)
{
    if (!m_store.setReturn(band, mode, entryId)) {
        setRefusal(refusal, kNoEntry);
        return false;
    }
    markChanged();
    return true;
}

bool BandStackController::clear(std::optional<Band> band, QString* /*refusal*/)
{
    if (m_store.clearUnlocked(band) > 0) {
        markChanged();
    }
    return true;
}

bool BandStackController::reseed(QString* /*refusal*/)
{
    m_store.replaceWithSeeds(seedEntries());
    m_store.setRegion(NereusSDR::regionName(currentRegion()));
    markChanged();
    return true;
}

bool BandStackController::options(std::optional<bool> dupes, std::optional<int> expiryMinutes,
                                  std::optional<int> dwellSeconds, QString* refusal)
{
    if (expiryMinutes.has_value() && *expiryMinutes != 0 && *expiryMinutes != 5
        && *expiryMinutes != 15 && *expiryMinutes != 30 && *expiryMinutes != 60) {
        setRefusal(refusal, QStringLiteral("Auto-expiry can be off, or 5, 15, 30 or 60 minutes."));
        return false;
    }
    if (dwellSeconds.has_value() && *dwellSeconds != 0 && *dwellSeconds != 10
        && *dwellSeconds != 30 && *dwellSeconds != 60) {
        setRefusal(refusal, QStringLiteral("Auto-save can be off, or 10, 30 or 60 seconds."));
        return false;
    }

    BandStackOptions next = m_store.options();
    next.dupeProtection = dupes.value_or(next.dupeProtection);
    next.expiryMinutes = expiryMinutes.value_or(next.expiryMinutes);
    next.dwellSeconds = dwellSeconds.value_or(next.dwellSeconds);
    m_store.setOptions(next);
    if (next.dwellSeconds == 0) {
        for (QTimer* timer : std::as_const(m_dwellTimers)) {
            timer->stop();
        }
    }
    markChanged();
    return true;
}

bool BandStackController::restoreExpired(const QString& batchId, QString* refusal)
{
    if (batchId.isEmpty() || batchId != m_lastExpiryBatchId
        || nowMs() - m_lastExpiryMs > kExpiryUndoMs) {
        setRefusal(refusal, QStringLiteral("Those entries can no longer be brought back."));
        return false;
    }

    // Restarts their time, so the next pass does not expire them straight away.
    QList<BandStackEntry> entries = m_lastExpired;
    const qint64 now = nowMs();
    for (BandStackEntry& entry : entries) {
        entry.lastUsedMs = now;
    }
    m_store.restore(entries);
    for (auto it = m_lastExpiredSpecific.constBegin(); it != m_lastExpiredSpecific.constEnd(); ++it) {
        m_store.setReturn(it.key(), BandStackReturnMode::Specific, it.value());
    }

    m_lastExpired.clear();
    m_lastExpiredSpecific.clear();
    m_lastExpiryBatchId.clear();
    m_lastExpiryCount = 0;
    markChanged();
    return true;
}

bool BandStackController::dwellArmedForTest(int sliceId) const
{
    const QTimer* timer = m_dwellTimers.value(sliceId, nullptr);
    return timer != nullptr && timer->isActive();
}

// What clients are told: the store, less the place of each detached slice on the
// band it tuned into. Every device then reads "no place there" with no new field,
// and the saved document keeps the place for the slice's next press.
QJsonObject BandStackController::publishedDocument() const
{
    QJsonObject document = m_store.toJson();
    if (m_detached.isEmpty()) {
        return document;
    }
    QJsonObject places = document.value(QStringLiteral("places")).toObject();
    for (const QChar letter : m_detached) {
        QJsonObject held = places.value(QString(letter)).toObject();
        held.remove(bandKeyName(m_onBand.value(letter)));
        places.insert(QString(letter), held);
    }
    document.insert(QStringLiteral("places"), places);
    return document;
}

QList<BandStackEntry> BandStackController::seedEntries() const
{
    const FilterPresetStore* presets = m_model->filterPresetStore();
    QList<BandStackEntry> entries;
    const QList<SeedRow> rows = seedsForRegion(currentRegion());
    entries.reserve(rows.size());
    for (const SeedRow& row : rows) {
        const std::optional<DSPMode> mode = knownMode(row.mode);
        if (!mode.has_value()) {
            qCWarning(lcApp) << "Band stack seed row not loaded, unknown mode:" << row.mode
                             << row.frequencyHz;
            continue;
        }

        // FN is the Nth slot of the mode's presets: the user's table, then the
        // built-in one if that has no such slot.
        const int slot = row.filterNumber - 1;
        std::optional<std::pair<int, int>> filter;
        if (presets != nullptr) {
            const QList<FilterPreset> table = presets->presetsForMode(*mode);
            if (slot >= 0 && slot < table.size()) {
                filter = std::make_pair(table.at(slot).low, table.at(slot).high);
            }
        }
        if (!filter.has_value()) {
            const QList<std::pair<int, int>> table = SliceModel::presetsForMode(*mode);
            if (slot >= 0 && slot < table.size()) {
                filter = table.at(slot);
            }
        }
        if (!filter.has_value()) {
            qCWarning(lcApp) << "Band stack seed row not loaded, no filter:" << row.mode
                             << row.filterNumber << row.frequencyHz;
            continue;
        }

        BandStackEntry entry;
        entry.band = row.band;
        entry.frequencyHz = row.frequencyHz;
        entry.mode = row.mode;
        entry.filterLowHz = filter->first;
        entry.filterHighHz = filter->second;
        entry.centerHz = row.frequencyHz;
        entry.locked = row.locked;
        entries.append(entry);
    }
    return entries;
}

qint64 BandStackController::nowMs() const
{
    return m_clockForTest ? m_clockForTest() : QDateTime::currentMSecsSinceEpoch();
}

void BandStackController::wireSlices()
{
    for (SliceModel* slice : m_model->slices()) {
        if (slice == nullptr || m_wiredSlices.contains(slice)) {
            continue;
        }
        m_wiredSlices.insert(slice);
        const auto tuned = [this, slice] { onSliceTuned(slice); };
        connect(slice, &SliceModel::frequencyChanged, this, tuned);
        connect(slice, &SliceModel::dspModeChanged, this, tuned);
        connect(slice, &SliceModel::filterChanged, this, tuned);
        const int sliceId = slice->sliceIndex();
        connect(slice, &QObject::destroyed, this, [this, slice, sliceId] {
            m_wiredSlices.remove(slice);
            stopDwell(sliceId);
        });
        if (m_adoptOpeningSlices) {
            adoptSavedSpots(slice);
            markChanged();
        }
        // A letter can come back as a new slice, which has crossed nothing yet.
        m_onBand[slice->sliceLetter()] = stackBandFor(slice);
        m_detached.remove(slice->sliceLetter());
    }
}

// Design V4: tuning, across a band edge or not, only updates this band's last
// frequency used and arms dwell. It saves no spot and moves no place. Arriving on
// another band this way detaches the slice from its place there (design 5.3).
void BandStackController::onSliceTuned(SliceModel* slice)
{
    if (m_inRecall) {
        return;
    }
    const QChar letter = slice->sliceLetter();
    const auto recalled = m_recalledBand.constFind(letter);
    if (recalled != m_recalledBand.constEnd()
        && recalled->gridBand != BandDefaults::bandGridBandFromFrequency(slice->frequency())) {
        m_recalledBand.remove(letter);
    }
    const Band band = stackBandFor(slice);
    m_lastUsed[letter][band] = currentSpot(slice, {}, nullptr);
    armDwell(slice);

    if (m_onBand.value(letter, band) != band) {
        m_onBand[letter] = band;
        m_detached.insert(letter);
        // The published document changed; the saved one did not.
        emit changed();
    }
}

void BandStackController::armDwell(const SliceModel* slice)
{
    const int seconds = m_store.options().dwellSeconds;
    if (seconds <= 0) {
        return;
    }
    const int sliceId = slice->sliceIndex();
    QTimer* timer = m_dwellTimers.value(sliceId, nullptr);
    if (timer == nullptr) {
        timer = new QTimer(this);
        timer->setSingleShot(true);
        connect(timer, &QTimer::timeout, this, [this, sliceId] { runDwell(sliceId); });
        m_dwellTimers.insert(sliceId, timer);
    }
    timer->start(seconds * 1000);
}

void BandStackController::stopDwell(int sliceId)
{
    QTimer* timer = m_dwellTimers.value(sliceId, nullptr);
    if (timer != nullptr) {
        timer->stop();
    }
}

void BandStackController::runDwell(int sliceId)
{
    const SliceModel* slice = m_model->sliceById(sliceId);
    if (slice == nullptr || m_store.options().dwellSeconds <= 0) {
        return;
    }
    if (m_store.addDwell(stackBandFor(slice), currentSpot(slice, {}, nullptr), nowMs())) {
        markChanged();
    }
}

void BandStackController::runExpiry()
{
    QMap<Band, QString> specific;
    for (int i = 0; i < static_cast<int>(Band::Count); ++i) {
        const Band band = static_cast<Band>(i);
        const QString id = m_store.specificId(band);
        if (!id.isEmpty()) {
            specific.insert(band, id);
        }
    }

    const QList<BandStackEntry> removed = m_store.expire(nowMs());
    if (removed.isEmpty()) {
        return;
    }

    // An entry leaving takes its band's "Always return here" with it, so the
    // undo has to put that back too.
    m_lastExpiredSpecific.clear();
    for (const BandStackEntry& entry : removed) {
        if (specific.value(entry.band) == entry.id) {
            m_lastExpiredSpecific.insert(entry.band, entry.id);
        }
    }
    m_lastExpired = removed;
    m_lastExpiryBatchId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_lastExpiryCount = static_cast<int>(removed.size());
    m_lastExpiryMs = nowMs();
    markChanged();
}

QString BandStackController::documentKey() const
{
    return QStringLiteral("hardware/%1/bandstack/Document").arg(m_mac);
}

void BandStackController::loadDocument()
{
    const QString mac = m_model->currentRadioInfo().macAddress;
    if (mac.isEmpty()) {
        return;
    }

    // The radio leaving keeps its own document, and a reconnect reads back
    // what was written.
    writeDocument();
    m_saveTimer->stop();
    m_mac = mac;
    m_lastUsed.clear();
    m_recalledBand.clear();
    m_detached.clear();
    m_adoptOpeningSlices = false;
    m_adopted.clear();
    m_lastExpired.clear();
    m_lastExpiredSpecific.clear();
    m_lastExpiryBatchId.clear();
    m_lastExpiryCount = 0;
    for (QTimer* timer : std::as_const(m_dwellTimers)) {
        timer->stop();
    }

    auto& settings = AppSettings::instance();
    const QString raw = settings.value(documentKey()).toString();
    if (raw.isEmpty()) {
        startFresh();
    } else {
        QJsonParseError error;
        const QJsonDocument document = QJsonDocument::fromJson(raw.toUtf8(), &error);
        BandStackStore loaded;
        if (error.error == QJsonParseError::NoError && document.isObject()
            && loaded.fromJson(document.object())) {
            m_store = std::move(loaded);
        } else {
            qCWarning(lcApp) << "Band stack: the saved document for" << m_mac
                             << "could not be read. It was kept as Document.bad and the "
                                "stack starts again from the region's seeds.";
            settings.setValue(documentKey() + QStringLiteral(".bad"), raw);
            startFresh();
        }
    }

    // The region is the Core's setting. It may have changed while this radio
    // was away; the entries stay until someone asks for the new region's seeds.
    const QString region = NereusSDR::regionName(currentRegion());
    if (m_store.region() != region) {
        m_store.setRegion(region);
        m_dirty = true;
    }
    if (m_dirty) {
        m_saveTimer->start();
    }
    // Nothing has crossed a band on this document yet.
    m_onBand.clear();
    for (const SliceModel* slice : m_model->slices()) {
        if (slice != nullptr) {
            m_onBand[slice->sliceLetter()] = stackBandFor(slice);
        }
    }
    emit changed();
}

// Design V8: the region's seeds, then each slice's saved spots. No slice moves.
void BandStackController::startFresh()
{
    m_store = BandStackStore();
    m_store.loadSeeds(seedEntries());
    m_store.setRegion(NereusSDR::regionName(currentRegion()));
    m_adoptOpeningSlices = true;
    for (const SliceModel* slice : m_model->slices()) {
        if (slice != nullptr) {
            adoptSavedSpots(slice);
        }
    }
    m_dirty = true;
}

void BandStackController::adoptSavedSpots(const SliceModel* slice)
{
    const QChar letter = slice->sliceLetter();
    if (m_adopted.contains(letter)) {
        return;
    }
    m_adopted.insert(letter);

    for (int i = 0; i < static_cast<int>(Band::Count); ++i) {
        const Band band = static_cast<Band>(i);
        if (band == Band::XVTR || !slice->hasSettingsFor(band)) {
            continue;
        }
        const std::optional<SliceModel::SavedBandSpot> saved = slice->savedBandSpot(band);
        if (!saved.has_value()) {
            continue;
        }
        const qint64 hz = qRound64(saved->frequencyHz);
        const QString existing = m_store.entryNear(band, hz);
        if (!existing.isEmpty()) {
            m_store.setPlace(letter, band, existing);
            continue;
        }
        const BandStackSpot spot{hz, SliceModel::modeName(saved->mode), saved->filterLow,
                                 saved->filterHigh, hz, 0};
        m_store.add(letter, band, spot, nowMs(), nullptr);
    }
}

void BandStackController::followRegion()
{
    const QString region = NereusSDR::regionName(currentRegion());
    if (m_store.region() == region) {
        return;
    }
    m_store.setRegion(region);
    markChanged();
}

void BandStackController::markChanged()
{
    m_dirty = true;
    // Not restarted: the document is written within a second of the first
    // change, however many follow.
    if (!m_saveTimer->isActive()) {
        m_saveTimer->start();
    }
    emit changed();
}

void BandStackController::flush()
{
    m_saveTimer->stop();
    writeDocument();
}

void BandStackController::writeDocument()
{
    if (!m_dirty || m_mac.isEmpty()) {
        return;
    }
    AppSettings::instance().setValue(
        documentKey(),
        QString::fromUtf8(QJsonDocument(m_store.toJson()).toJson(QJsonDocument::Compact)));
    m_dirty = false;
}

} // namespace NereusSDR
