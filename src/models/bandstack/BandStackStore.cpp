// =================================================================
// src/models/bandstack/BandStackStore.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/clsBandStackManager.cs [v2.10.3.15 @3759d09],
//   Project Files/Source/Console/console.cs [v2.10.3.15 @3759d09],
//   original licence from Thetis source is included below
//
// The band stack's rules. The cycle, the return modes and the lock and
// duplicate checks come from BandStackFilter and preBandSelect. The design
// differs where NereusSDR has several slices on one shared list; each
// difference is marked below with why ours differs. The list of them is
// section 2 of docs/architecture/2026-11-05-band-stack-design.md.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-05 - Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
// =================================================================

// --- From clsBandStackManager.cs ---
/*  clsBandStackManager.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2020-2026 Richard Samphire MW0LGE

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at

mw0lge@grange-lane.co.uk
*/
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

// --- From console.cs ---
//=================================================================
// console.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems 
// Copyright (C) 2010-2020  Doug Wigley
// Credit is given to Sizenko Alexander of Style-7 (http://www.styleseven.com/) for the Digital-7 font.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// You may contact us via email at: sales@flex-radio.com.
// Paper mail may be sent to: 
//    FlexRadio Systems
//    8900 Marybank Dr.
//    Austin, TX 78750
//    USA
//
//=================================================================
// Modifications to support the Behringer Midi controllers
// by Chris Codella, W2PA, May 2017.  Indicated by //-W2PA comment lines. 
// Modifications for using the new database import function.  W2PA, 29 May 2017
// Support QSK, possible with Protocol-2 firmware v1.7 (Orion-MkI and Orion-MkII), and later.  W2PA, 5 April 2019 
// Modfied heavily - Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
// ApacheLabs G2E support added throughout Thetis in various files, all changes marked  //N1GP G2E added
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Final modifictions by MW0LGE Richard Samphire - 19th April 2026
// Nothing further added by him after this date, and his repo is now in archive https://github.com/ramdor/Thetis
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Migrated to VS2026 - 18/12/25 MW0LGE v2.10.3.12

#include "models/bandstack/BandStackStore.h"

#include <QJsonArray>
#include <QJsonValue>
#include <QLoggingCategory>
#include <QUuid>

#include <algorithm>
#include <iterator>
#include <limits>
#include <utility>

#include "core/LogCategories.h"

namespace NereusSDR {

namespace {

constexpr int kSchemaVersion = 1;

// Ours differs from Thetis, which compares exactly (FindEntriesForFrequency,
// clsBandStackManager.cs:590-595 [v2.10.3.15]): two spots 50 Hz apart are the
// same spot to an operator. Exactly 100 Hz apart is not a duplicate.
constexpr qint64 kDuplicateWindowHz = 100;

constexpr qint64 kMsPerMinute = 60000;

QString newEntryId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

bool withinDuplicateWindow(qint64 a, qint64 b)
{
    return qAbs(a - b) < kDuplicateWindowHz;
}

bool isVoiceMode(const QString& mode)
{
    return mode == QLatin1String("LSB") || mode == QLatin1String("USB");
}

bool isAllowedExpiry(int minutes)
{
    return minutes == 0 || minutes == 5 || minutes == 15 || minutes == 30 || minutes == 60;
}

bool isAllowedDwell(int seconds)
{
    return seconds == 0 || seconds == 10 || seconds == 30 || seconds == 60;
}

// The same shape the refusal shows everywhere else: 7.185 or 7.185.250.
QString displayFrequency(qint64 frequencyHz)
{
    const QString text = QString::number(static_cast<double>(frequencyHz) / 1e6, 'f', 6);
    const qsizetype split = text.indexOf(QLatin1Char('.')) + 4;
    const QString hertz = text.mid(split);
    if (hertz == QLatin1String("000")) {
        return text.left(split);
    }
    return text.left(split) + QLatin1Char('.') + hertz;
}

// From Thetis clsBandStackManager.cs:391-402 [v2.10.3.15]
void writeSpot(BandStackEntry& entry, const BandStackSpot& spot, qint64 nowMs)
{
    const bool changed = entry.frequencyHz != spot.frequencyHz || entry.mode != spot.mode
                         || entry.filterLowHz != spot.filterLowHz
                         || entry.filterHighHz != spot.filterHighHz
                         || entry.centerHz != spot.centerHz || entry.spanHz != spot.spanHz;
    entry.frequencyHz = spot.frequencyHz;
    entry.mode = spot.mode;
    entry.filterLowHz = spot.filterLowHz;
    entry.filterHighHz = spot.filterHighHz;
    entry.centerHz = spot.centerHz;
    entry.spanHz = spot.spanHz;
    entry.lastUsedMs = nowMs;
    if (changed) {
        // A saved change makes the entry the user's (design section 5.3).
        entry.seeded = false;
    }
}

QString returnModeName(BandStackReturnMode mode)
{
    switch (mode) {
    case BandStackReturnMode::LastUsed:
        return QStringLiteral("lastUsed");
    case BandStackReturnMode::Specific:
        return QStringLiteral("specific");
    case BandStackReturnMode::Current:
        break;
    }
    return QStringLiteral("current");
}

std::optional<BandStackReturnMode> returnModeFromName(const QString& name)
{
    if (name == QLatin1String("current")) {
        return BandStackReturnMode::Current;
    }
    if (name == QLatin1String("lastUsed")) {
        return BandStackReturnMode::LastUsed;
    }
    if (name == QLatin1String("specific")) {
        return BandStackReturnMode::Specific;
    }
    return std::nullopt;
}

// Not bandFromName, which knows no SWL band and files any unknown string under
// GEN. A document's key names exactly the band bandKeyName wrote it for.
std::optional<Band> strictBandFromKey(const QString& key)
{
    for (int i = 0; i < static_cast<int>(Band::Count); ++i) {
        const Band band = static_cast<Band>(i);
        if (bandKeyName(band) == key) {
            return band;
        }
    }
    return std::nullopt;
}

std::optional<qint64> readInteger(const QJsonObject& object, const QString& key)
{
    const QJsonValue value = object.value(key);
    constexpr qint64 kNotInteger = std::numeric_limits<qint64>::min();
    const qint64 integer = value.toInteger(kNotInteger);
    if (!value.isDouble() || integer == kNotInteger) {
        return std::nullopt;
    }
    return integer;
}

std::optional<int> readInt(const QJsonObject& object, const QString& key)
{
    const std::optional<qint64> integer = readInteger(object, key);
    if (!integer.has_value() || *integer < std::numeric_limits<int>::min()
        || *integer > std::numeric_limits<int>::max()) {
        return std::nullopt;
    }
    return static_cast<int>(*integer);
}

std::optional<BandStackEntry> entryFromJson(const QJsonObject& object)
{
    const QJsonValue id = object.value(QStringLiteral("id"));
    const QJsonValue bandKey = object.value(QStringLiteral("band"));
    const QJsonValue mode = object.value(QStringLiteral("mode"));
    const QJsonValue locked = object.value(QStringLiteral("locked"));
    const QJsonValue seeded = object.value(QStringLiteral("seeded"));
    const QJsonValue name = object.value(QStringLiteral("name"));
    if (!id.isString() || id.toString().isEmpty() || !bandKey.isString() || !mode.isString()
        || !locked.isBool() || !seeded.isBool() || !name.isString()) {
        return std::nullopt;
    }

    const std::optional<Band> band = strictBandFromKey(bandKey.toString());
    const std::optional<qint64> frequencyHz = readInteger(object, QStringLiteral("frequencyHz"));
    const std::optional<int> filterLowHz = readInt(object, QStringLiteral("filterLowHz"));
    const std::optional<int> filterHighHz = readInt(object, QStringLiteral("filterHighHz"));
    const std::optional<qint64> centerHz = readInteger(object, QStringLiteral("centerHz"));
    const std::optional<qint64> spanHz = readInteger(object, QStringLiteral("spanHz"));
    const std::optional<qint64> lastUsedMs = readInteger(object, QStringLiteral("lastUsedMs"));
    if (!band.has_value() || !frequencyHz.has_value() || !filterLowHz.has_value()
        || !filterHighHz.has_value() || !centerHz.has_value() || !spanHz.has_value()
        || !lastUsedMs.has_value()) {
        return std::nullopt;
    }

    BandStackEntry entry;
    entry.id = id.toString();
    entry.band = *band;
    entry.frequencyHz = *frequencyHz;
    entry.mode = mode.toString();
    entry.filterLowHz = *filterLowHz;
    entry.filterHighHz = *filterHighHz;
    entry.centerHz = *centerHz;
    entry.spanHz = *spanHz;
    entry.locked = locked.toBool();
    entry.seeded = seeded.toBool();
    entry.name = name.toString();
    entry.lastUsedMs = *lastUsedMs;
    return entry;
}

QJsonObject entryToJson(const BandStackEntry& entry)
{
    QJsonObject object;
    object.insert(QStringLiteral("id"), entry.id);
    object.insert(QStringLiteral("band"), bandKeyName(entry.band));
    object.insert(QStringLiteral("frequencyHz"), entry.frequencyHz);
    object.insert(QStringLiteral("mode"), entry.mode);
    object.insert(QStringLiteral("filterLowHz"), entry.filterLowHz);
    object.insert(QStringLiteral("filterHighHz"), entry.filterHighHz);
    object.insert(QStringLiteral("centerHz"), entry.centerHz);
    object.insert(QStringLiteral("spanHz"), entry.spanHz);
    object.insert(QStringLiteral("locked"), entry.locked);
    object.insert(QStringLiteral("seeded"), entry.seeded);
    object.insert(QStringLiteral("name"), entry.name);
    object.insert(QStringLiteral("lastUsedMs"), entry.lastUsedMs);
    return object;
}

} // namespace

// From Thetis console.cs:45621-45734 [v2.10.3.15]
BandStackTarget BandStackStore::planPress(QChar slice, Band currentBand, Band band,
                                          BandStackDirection d) const
{
    //[2.10.3.6]MW0LGE no band change on TX fix [original inline comment from console.cs:45623]
    // The transmit refusal itself is the controller's: the store knows no slices.

    const auto state = m_bands.constFind(band);
    const bool empty = state == m_bands.constEnd() || state->order.isEmpty();

    if (currentBand == band) { // already on the band [original inline comment from console.cs:45662]
        if (empty) {
            // in case of nothing in the filter (ie deleted everything, or no entries)
            // [original inline comment from console.cs:45708]
            return {BandStackTarget::EmptyBand, QString(), band};
        }
        return planStep(slice, band, d);
    }

    // From Thetis clsBandStackManager.cs:502-505 [v2.10.3.15]: last visited does
    // not need the list, so it is asked before the list is checked.
    if (state != m_bands.constEnd() && state->returnMode == BandStackReturnMode::LastUsed) {
        return {BandStackTarget::LastUsed, QString(), band};
    }

    if (empty) {
        // in case of nothing in the filter (ie deleted everything, or no entries)
        // [original inline comment from console.cs:45708]
        return {BandStackTarget::EmptyBand, QString(), band};
    }

    // From Thetis clsBandStackManager.cs:486-501 [v2.10.3.15]
    if (state->returnMode == BandStackReturnMode::Specific) {
        if (m_entries.contains(state->specificId)) {
            return {BandStackTarget::Entry, state->specificId, band};
        }
        // failed to find this guid, just return to current mode
        // [original inline comment from clsBandStackManager.cs:490]
    }

    return currentTarget(slice, band);
}

// From Thetis clsBandStackManager.cs:551-587 [v2.10.3.15]
// Ours differs: one list is shared, so the index comes from the slice's own
// place, where Thetis keeps one index for RX1. Thetis's band keys walk off the
// end into the next band (console.cs:27007-27056 [v2.10.3.15]); ours wraps (V3).
BandStackTarget BandStackStore::planStep(QChar slice, Band band, BandStackDirection d) const
{
    const auto state = m_bands.constFind(band);
    if (state == m_bands.constEnd() || state->order.isEmpty()) {
        return {BandStackTarget::None, QString(), band};
    }

    const QStringList& order = state->order;
    const qsizetype count = order.size();
    const qsizetype index = order.indexOf(place(slice, band));
    qsizetype next = 0;
    if (index >= 0) {
        next = d == BandStackDirection::Next ? (index + 1) % count : (index + count - 1) % count;
    }
    return {BandStackTarget::Entry, order.at(next), band};
}

// Ours differs: the first press on a band with no place lands on the first
// voice entry, matching the choice in issue 118. Thetis uses index 0 of its
// list, which is often CW (clsBandStackManager.cs:476 [v2.10.3.15]).
BandStackTarget BandStackStore::currentTarget(QChar slice, Band band) const
{
    const QString ownPlace = place(slice, band);
    if (!ownPlace.isEmpty()) {
        return {BandStackTarget::Entry, ownPlace, band};
    }

    const QStringList order = m_bands.value(band).order;
    for (const QString& id : order) {
        if (isVoiceMode(m_entries.value(id).mode)) {
            return {BandStackTarget::Entry, id, band};
        }
    }
    return {BandStackTarget::Entry, order.first(), band};
}

// From Thetis clsBandStackManager.cs:366-414 [v2.10.3.15]
bool BandStackStore::saveOnLeave(QChar slice, Band band, const BandStackSpot& spot, qint64 nowMs,
                                 const QSet<QChar>& slicesOnBand)
{
    const QString id = place(slice, band);
    const auto entry = m_entries.find(id);
    if (entry == m_entries.end()) {
        return false;
    }

    if (entry->locked) {
        return false; // only do if unlocked [original inline comment from clsBandStackManager.cs:370]
    }

    // Ours differs: Thetis has one writer. A spot another slice is on is not
    // saved over; the last slice on it writes when it leaves (design section 5.3).
    for (auto other = m_places.constBegin(); other != m_places.constEnd(); ++other) {
        if (other.key() != slice && slicesOnBand.contains(other.key())
            && other.value().value(band) == id) {
            return false;
        }
    }

    if (m_options.dupeProtection && isDuplicate(band, spot.frequencyHz, id)) {
        return false; // abort if there is an entry that has the same frequency at a different index
                      // [original inline comment from clsBandStackManager.cs:386]
    }

    writeSpot(*entry, spot, nowMs);
    return true;
}

bool BandStackStore::saveHere(QChar slice, const QString& entryId, const BandStackSpot& spot,
                              qint64 nowMs)
{
    const auto entry = m_entries.find(entryId);
    if (entry == m_entries.end() || entry->locked) {
        return false;
    }
    writeSpot(*entry, spot, nowMs);
    m_places[slice][entry->band] = entryId;
    return true;
}

QString BandStackStore::add(QChar slice, Band band, const BandStackSpot& spot, qint64 nowMs,
                            QString* refusal)
{
    if (m_options.dupeProtection && isDuplicate(band, spot.frequencyHz, QString())) {
        if (refusal != nullptr) {
            *refusal = QStringLiteral("%1 is already in %2")
                           .arg(displayFrequency(spot.frequencyHz), bandKeyName(band));
        }
        return QString();
    }
    const BandStackEntry entry = addEntry(band, spot, nowMs);
    m_places[slice][band] = entry.id;
    return entry.id;
}

bool BandStackStore::addDwell(Band band, const BandStackSpot& spot, qint64 nowMs)
{
    if (m_options.dupeProtection && isDuplicate(band, spot.frequencyHz, QString())) {
        return false;
    }
    if (isDuplicateInMode(band, spot.frequencyHz, spot.mode)) {
        return false;
    }
    addEntry(band, spot, nowMs);
    return true;
}

void BandStackStore::setPlace(QChar slice, Band band, const QString& entryId)
{
    const auto entry = m_entries.constFind(entryId);
    if (entry != m_entries.constEnd() && entry->band == band) {
        m_places[slice][band] = entryId;
    }
}

void BandStackStore::clearPlace(QChar slice, Band band)
{
    const auto places = m_places.find(slice);
    if (places == m_places.end()) {
        return;
    }
    places->remove(band);
    if (places->isEmpty()) {
        m_places.erase(places);
    }
}

QString BandStackStore::place(QChar slice, Band band) const
{
    return m_places.value(slice).value(band);
}

bool BandStackStore::edit(const QString& id, const std::optional<QString>& name,
                          std::optional<bool> locked)
{
    const auto entry = m_entries.find(id);
    if (entry == m_entries.end()) {
        return false;
    }
    if (name.has_value()) {
        entry->name = *name;
    }
    if (locked.has_value()) {
        // V2: locking by hand makes a seed the user's. A seed already locked,
        // such as WWV, stays a seed when an edit merely repeats the lock.
        if (*locked && !entry->locked) {
            entry->seeded = false;
        }
        entry->locked = *locked;
    }
    return true;
}

bool BandStackStore::remove(const QString& id)
{
    if (!m_entries.contains(id)) {
        return false;
    }
    eraseEntry(id);
    return true;
}

bool BandStackStore::reorder(Band band, const QStringList& ids)
{
    const QStringList current = m_bands.value(band).order;
    QStringList sortedIds = ids;
    QStringList sortedCurrent = current;
    std::sort(sortedIds.begin(), sortedIds.end());
    std::sort(sortedCurrent.begin(), sortedCurrent.end());
    if (sortedIds != sortedCurrent) {
        return false;
    }
    m_bands[band].order = ids;
    return true;
}

bool BandStackStore::setReturn(Band band, BandStackReturnMode mode, const QString& entryId)
{
    if (mode == BandStackReturnMode::Specific) {
        const auto entry = m_entries.constFind(entryId);
        if (entry == m_entries.constEnd() || entry->band != band) {
            return false;
        }
    }
    BandState& state = m_bands[band];
    state.returnMode = mode;
    state.specificId = mode == BandStackReturnMode::Specific ? entryId : QString();
    return true;
}

int BandStackStore::clearUnlocked(std::optional<Band> band)
{
    QStringList doomed;
    for (const BandStackEntry& entry : std::as_const(m_entries)) {
        if (!entry.locked && (!band.has_value() || entry.band == *band)) {
            doomed.append(entry.id);
        }
    }
    for (const QString& id : std::as_const(doomed)) {
        eraseEntry(id);
    }
    return static_cast<int>(doomed.size());
}

void BandStackStore::loadSeeds(const QList<BandStackEntry>& seeds)
{
    m_entries.clear();
    m_bands.clear();
    m_places.clear();
    addSeeds(seeds);
}

void BandStackStore::replaceWithSeeds(const QList<BandStackEntry>& seeds)
{
    QStringList doomed;
    for (const BandStackEntry& entry : std::as_const(m_entries)) {
        if (!entry.locked || entry.seeded) {
            doomed.append(entry.id);
        }
    }
    for (const QString& id : std::as_const(doomed)) {
        eraseEntry(id);
    }
    addSeeds(seeds);
}

QList<BandStackEntry> BandStackStore::expire(qint64 nowMs)
{
    QList<BandStackEntry> removed;
    if (m_options.expiryMinutes <= 0) {
        return removed;
    }

    QSet<QString> inPlace;
    for (const QMap<Band, QString>& places : std::as_const(m_places)) {
        for (const QString& id : places) {
            inPlace.insert(id);
        }
    }

    const qint64 windowMs = m_options.expiryMinutes * kMsPerMinute;
    for (auto band = m_bands.constBegin(); band != m_bands.constEnd(); ++band) {
        for (const QString& id : band.value().order) {
            const BandStackEntry& entry = m_entries[id];
            if (!entry.locked && !entry.seeded && !inPlace.contains(id)
                && nowMs - entry.lastUsedMs >= windowMs) {
                removed.append(entry);
            }
        }
    }
    for (const BandStackEntry& entry : std::as_const(removed)) {
        eraseEntry(entry.id);
    }
    return removed;
}

void BandStackStore::restore(const QList<BandStackEntry>& entries)
{
    for (const BandStackEntry& entry : entries) {
        if (entry.id.isEmpty() || m_entries.contains(entry.id)) {
            continue;
        }
        m_entries.insert(entry.id, entry);
        insertInOrder(entry.id, entry.band, entry.frequencyHz);
    }
}

QList<BandStackEntry> BandStackStore::entries(Band band) const
{
    QList<BandStackEntry> list;
    const QStringList order = m_bands.value(band).order;
    list.reserve(order.size());
    for (const QString& id : order) {
        list.append(m_entries.value(id));
    }
    return list;
}

const BandStackEntry* BandStackStore::entry(const QString& id) const
{
    const auto found = m_entries.constFind(id);
    return found == m_entries.constEnd() ? nullptr : &found.value();
}

BandStackOptions BandStackStore::options() const
{
    return m_options;
}

void BandStackStore::setOptions(const BandStackOptions& options)
{
    m_options.dupeProtection = options.dupeProtection;
    if (isAllowedExpiry(options.expiryMinutes)) {
        m_options.expiryMinutes = options.expiryMinutes;
    } else {
        qCWarning(lcApp) << "Band stack expiry not one of 0, 5, 15, 30, 60:" << options.expiryMinutes;
    }
    if (isAllowedDwell(options.dwellSeconds)) {
        m_options.dwellSeconds = options.dwellSeconds;
    } else {
        qCWarning(lcApp) << "Band stack dwell not one of 0, 10, 30, 60:" << options.dwellSeconds;
    }
}

QString BandStackStore::region() const
{
    return m_region;
}

void BandStackStore::setRegion(const QString& region)
{
    m_region = region;
}

QJsonObject BandStackStore::toJson() const
{
    QJsonObject options;
    options.insert(QStringLiteral("dupeProtection"), m_options.dupeProtection);
    options.insert(QStringLiteral("expiryMinutes"), m_options.expiryMinutes);
    options.insert(QStringLiteral("dwellSeconds"), m_options.dwellSeconds);

    QJsonObject bands;
    QJsonArray entries;
    for (auto band = m_bands.constBegin(); band != m_bands.constEnd(); ++band) {
        const BandState& state = band.value();
        if (state.order.isEmpty() && state.returnMode == BandStackReturnMode::Current) {
            continue; // the same as a band the document does not mention
        }
        QJsonObject object;
        object.insert(QStringLiteral("order"), QJsonArray::fromStringList(state.order));
        object.insert(QStringLiteral("returnMode"), returnModeName(state.returnMode));
        object.insert(QStringLiteral("specificId"), state.specificId);
        bands.insert(bandKeyName(band.key()), object);

        for (const QString& id : state.order) {
            entries.append(entryToJson(m_entries[id]));
        }
    }

    QJsonObject places;
    for (auto slice = m_places.constBegin(); slice != m_places.constEnd(); ++slice) {
        QJsonObject object;
        for (auto band = slice.value().constBegin(); band != slice.value().constEnd(); ++band) {
            object.insert(bandKeyName(band.key()), band.value());
        }
        places.insert(QString(slice.key()), object);
    }

    QJsonObject document;
    document.insert(QStringLiteral("schema"), kSchemaVersion);
    document.insert(QStringLiteral("region"), m_region);
    document.insert(QStringLiteral("options"), options);
    document.insert(QStringLiteral("bands"), bands);
    document.insert(QStringLiteral("entries"), entries);
    document.insert(QStringLiteral("places"), places);
    return document;
}

bool BandStackStore::fromJson(const QJsonObject& document)
{
    const std::optional<int> schema = readInt(document, QStringLiteral("schema"));
    const QJsonValue region = document.value(QStringLiteral("region"));
    const QJsonValue optionsValue = document.value(QStringLiteral("options"));
    const QJsonValue bandsValue = document.value(QStringLiteral("bands"));
    const QJsonValue entriesValue = document.value(QStringLiteral("entries"));
    const QJsonValue placesValue = document.value(QStringLiteral("places"));
    if (schema != kSchemaVersion || !region.isString() || !optionsValue.isObject()
        || !bandsValue.isObject() || !entriesValue.isArray() || !placesValue.isObject()) {
        return false;
    }

    BandStackStore parsed;
    parsed.m_region = region.toString();

    const QJsonObject options = optionsValue.toObject();
    const QJsonValue dupeProtection = options.value(QStringLiteral("dupeProtection"));
    const std::optional<int> expiryMinutes = readInt(options, QStringLiteral("expiryMinutes"));
    const std::optional<int> dwellSeconds = readInt(options, QStringLiteral("dwellSeconds"));
    if (!dupeProtection.isBool() || !expiryMinutes.has_value() || !isAllowedExpiry(*expiryMinutes)
        || !dwellSeconds.has_value() || !isAllowedDwell(*dwellSeconds)) {
        return false;
    }
    parsed.m_options = {dupeProtection.toBool(), *expiryMinutes, *dwellSeconds};

    // Entries come first: a band's order and specificId and a slice's place
    // are only meaningful against them.
    QStringList documentOrder;
    const QJsonArray entries = entriesValue.toArray();
    for (const QJsonValue& value : entries) {
        const std::optional<BandStackEntry> entry =
            value.isObject() ? entryFromJson(value.toObject()) : std::nullopt;
        if (!entry.has_value() || parsed.m_entries.contains(entry->id)) {
            return false;
        }
        parsed.m_entries.insert(entry->id, *entry);
        documentOrder.append(entry->id);
    }

    const QJsonObject bands = bandsValue.toObject();
    for (auto it = bands.constBegin(); it != bands.constEnd(); ++it) {
        const std::optional<Band> band = strictBandFromKey(it.key());
        if (!band.has_value() || !it.value().isObject()) {
            return false;
        }
        const QJsonObject object = it.value().toObject();
        const QJsonValue order = object.value(QStringLiteral("order"));
        const QJsonValue specificId = object.value(QStringLiteral("specificId"));
        const std::optional<BandStackReturnMode> returnMode =
            returnModeFromName(object.value(QStringLiteral("returnMode")).toString());
        if (!order.isArray() || !specificId.isString() || !returnMode.has_value()) {
            return false;
        }

        BandState& state = parsed.m_bands[*band];
        state.returnMode = *returnMode;
        const QJsonArray ids = order.toArray();
        for (const QJsonValue& id : ids) {
            if (!id.isString()) {
                return false;
            }
            // An id with no entry, on another band or repeated is left out.
            const auto entry = parsed.m_entries.constFind(id.toString());
            if (entry != parsed.m_entries.constEnd() && entry->band == *band
                && !state.order.contains(id.toString())) {
                state.order.append(id.toString());
            }
        }

        // A specific entry that has gone sends the band back to Current, as a
        // remove does.
        const auto specific = parsed.m_entries.constFind(specificId.toString());
        if (state.returnMode == BandStackReturnMode::Specific) {
            if (specific != parsed.m_entries.constEnd() && specific->band == *band) {
                state.specificId = specificId.toString();
            } else {
                state.returnMode = BandStackReturnMode::Current;
            }
        }
    }

    // An entry that no order names goes where a new one would, so a band with
    // no order at all comes out in frequency order.
    for (const QString& id : std::as_const(documentOrder)) {
        const BandStackEntry& entry = parsed.m_entries[id];
        if (!parsed.m_bands[entry.band].order.contains(id)) {
            parsed.insertInOrder(id, entry.band, entry.frequencyHz);
        }
    }

    const QJsonObject places = placesValue.toObject();
    for (auto slice = places.constBegin(); slice != places.constEnd(); ++slice) {
        if (slice.key().size() != 1 || !slice.value().isObject()) {
            return false;
        }
        const QJsonObject object = slice.value().toObject();
        for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
            const std::optional<Band> band = strictBandFromKey(it.key());
            if (!band.has_value() || !it.value().isString()) {
                return false;
            }
            // A place on an entry that has gone is no place.
            parsed.setPlace(slice.key().at(0), *band, it.value().toString());
        }
    }

    *this = std::move(parsed);
    return true;
}

// Ours differs: within 100 Hz, not exact. See kDuplicateWindowHz.
// From Thetis clsBandStackManager.cs:372-389 [v2.10.3.15]
bool BandStackStore::isDuplicate(Band band, qint64 frequencyHz, const QString& excludeId) const
{
    const QStringList order = m_bands.value(band).order;
    return std::any_of(order.cbegin(), order.cend(), [&](const QString& id) {
        return id != excludeId && withinDuplicateWindow(m_entries[id].frequencyHz, frequencyHz);
    });
}

bool BandStackStore::isDuplicateInMode(Band band, qint64 frequencyHz, const QString& mode) const
{
    const QStringList order = m_bands.value(band).order;
    return std::any_of(order.cbegin(), order.cend(), [&](const QString& id) {
        const BandStackEntry& entry = m_entries[id];
        return entry.mode == mode && withinDuplicateWindow(entry.frequencyHz, frequencyHz);
    });
}

BandStackEntry BandStackStore::addEntry(Band band, const BandStackSpot& spot, qint64 nowMs)
{
    BandStackEntry entry;
    entry.id = newEntryId();
    entry.band = band;
    entry.frequencyHz = spot.frequencyHz;
    entry.mode = spot.mode;
    entry.filterLowHz = spot.filterLowHz;
    entry.filterHighHz = spot.filterHighHz;
    entry.centerHz = spot.centerHz;
    entry.spanHz = spot.spanHz;
    entry.lastUsedMs = nowMs;
    m_entries.insert(entry.id, entry);
    insertInOrder(entry.id, band, entry.frequencyHz);
    return entry;
}

// Ours differs: dragging can reorder a band, where Thetis always sorts by
// frequency (GenerateFilteredList, clsBandStackManager.cs:714-721 [v2.10.3.15]).
// A new spot goes before the first entry with a higher frequency, or last.
void BandStackStore::insertInOrder(const QString& id, Band band, qint64 frequencyHz)
{
    QStringList& order = m_bands[band].order;
    const auto position = std::find_if(order.begin(), order.end(), [&](const QString& other) {
        return m_entries.value(other).frequencyHz > frequencyHz;
    });
    order.insert(position, id);
}

void BandStackStore::eraseEntry(const QString& id)
{
    const auto entry = m_entries.constFind(id);
    if (entry == m_entries.constEnd()) {
        return;
    }
    const Band band = entry->band;

    for (auto places = m_places.begin(); places != m_places.end();) {
        if (places->value(band) == id) {
            places->remove(band);
        }
        places = places->isEmpty() ? m_places.erase(places) : std::next(places);
    }

    BandState& state = m_bands[band];
    state.order.removeOne(id);
    if (state.specificId == id) {
        // From Thetis clsBandStackManager.cs:490-491 [v2.10.3.15]
        state.specificId.clear();
        state.returnMode = BandStackReturnMode::Current;
    }

    m_entries.remove(id);
}

void BandStackStore::addSeeds(const QList<BandStackEntry>& seeds)
{
    for (BandStackEntry seed : seeds) {
        if (seed.id.isEmpty() || m_entries.contains(seed.id)) {
            seed.id = newEntryId();
        }
        seed.seeded = true;
        m_entries.insert(seed.id, seed);
        insertInOrder(seed.id, seed.band, seed.frequencyHz);
    }
}

} // namespace NereusSDR
