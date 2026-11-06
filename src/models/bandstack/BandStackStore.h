// =================================================================
// src/models/bandstack/BandStackStore.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/clsBandStackManager.cs [v2.10.3.15 @3759d09],
//   Project Files/Source/Console/console.cs [v2.10.3.15 @3759d09],
//   original licence from Thetis source is included below
//
// The band stack's data and rules: entries, per-band order, return modes
// and each slice's place. See BandStackStore.cpp.
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

#pragma once

#include <QChar>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QtGlobal>

#include <optional>

#include "models/Band.h"

namespace NereusSDR {

/// One saved spot on a band. `spanHz` 0 means no saved zoom.
struct BandStackEntry {
    QString id;
    Band band{Band::GEN};
    qint64 frequencyHz{0};
    QString mode;
    int filterLowHz{0};
    int filterHighHz{0};
    qint64 centerHz{0};
    qint64 spanHz{0};
    bool locked{false};
    bool seeded{false};
    QString name;
    qint64 lastUsedMs{0};
};

/// A slice's state at the moment it is saved into the stack.
struct BandStackSpot {
    qint64 frequencyHz{0};
    QString mode;
    int filterLowHz{0};
    int filterHighHz{0};
    qint64 centerHz{0};
    qint64 spanHz{0};
};

enum class BandStackReturnMode { Current, LastUsed, Specific };
enum class BandStackDirection { Next, Previous };

/// Where a press leads. `None` means there is nothing to step to.
struct BandStackTarget {
    enum Kind { Entry, LastUsed, EmptyBand, None } kind{None};
    QString entryId;
    Band band{Band::GEN};
};

struct BandStackOptions {
    bool dupeProtection{true};
    int expiryMinutes{0};
    int dwellSeconds{0};
};

/// Plain data and rules with no slices, no link and no widgets. It decides
/// what a press means; the controller acts on the answer. Main thread only.
class BandStackStore {
public:
    BandStackTarget planPress(QChar slice, Band currentBand, Band band, BandStackDirection d) const;
    BandStackTarget planStep(QChar slice, Band band, BandStackDirection d) const;

    /// What Current entry would choose on `band`: the slice's place, or the
    /// first voice entry, or the first entry, or EmptyBand with no entries.
    BandStackTarget planCurrent(QChar slice, Band band) const;

    /// Writes `spot` into the slice's place on `band` unless the entry is
    /// locked, another slice in `slicesOnBand` has it as its place, or
    /// duplicate protection finds another entry within 100 Hz. Returns true
    /// when it wrote.
    bool saveOnLeave(QChar slice, Band band, const BandStackSpot& spot, qint64 nowMs,
                     const QSet<QChar>& slicesOnBand);

    /// The deliberate save: refused only on a locked or unknown entry. The
    /// entry becomes the slice's place; no other slice's place changes.
    bool saveHere(QChar slice, const QString& entryId, const BandStackSpot& spot, qint64 nowMs);

    /// Adds a new entry and makes it the slice's place. Returns its id, or an
    /// empty string with `refusal` set (when non-null) if it is a duplicate.
    QString add(QChar slice, Band band, const BandStackSpot& spot, qint64 nowMs, QString* refusal);

    /// Adds a dwell save. No place changes. Returns true when it added.
    bool addDwell(Band band, const BandStackSpot& spot, qint64 nowMs);

    /// A recall used the entry: it restarts its expiry time. Unknown is ignored.
    void markUsed(const QString& id, qint64 nowMs);

    /// The first entry on `band` within 100 Hz of `frequencyHz`, or empty.
    QString entryNear(Band band, qint64 frequencyHz) const;

    /// The band's "Always return here" entry, or empty.
    QString specificId(Band band) const;

    /// Ignored unless `entryId` is an entry on `band`.
    void setPlace(QChar slice, Band band, const QString& entryId);
    void clearPlace(QChar slice, Band band);
    QString place(QChar slice, Band band) const;

    /// Locking an unlocked entry makes it the user's: it stops being a seed.
    bool edit(const QString& id, const std::optional<QString>& name, std::optional<bool> locked);
    bool remove(const QString& id);
    bool reorder(Band band, const QStringList& ids);

    /// `entryId` matters only for Specific, which needs an entry on `band`.
    bool setReturn(Band band, BandStackReturnMode mode, const QString& entryId);

    /// Removes unlocked entries from `band`, or from every band when nullopt.
    /// Returns how many went.
    int clearUnlocked(std::optional<Band> band);

    /// Replaces the whole stack with `seeds`, every one marked seeded and
    /// keeping its lock. Options and region stay.
    void loadSeeds(const QList<BandStackEntry>& seeds);

    /// Region change: removes everything except entries locked and no longer
    /// seeded, then adds `seeds` as loadSeeds does, leaving out any seed within
    /// 100 Hz of a kept entry on the same band.
    void replaceWithSeeds(const QList<BandStackEntry>& seeds);

    /// Removes and returns entries past the expiry time, skipping locked,
    /// seeded and any entry that is a slice's place.
    QList<BandStackEntry> expire(qint64 nowMs);

    /// Puts entries back in frequency order. An id already present is skipped.
    void restore(const QList<BandStackEntry>& entries);

    /// The band's entries in stored order.
    QList<BandStackEntry> entries(Band band) const;

    /// Null when unknown. The pointer is good until the next change.
    const BandStackEntry* entry(const QString& id) const;

    BandStackOptions options() const;

    /// A value the document does not allow is logged and that option keeps
    /// its previous value.
    void setOptions(const BandStackOptions& options);

    /// The region name the document records, opaque to the store.
    QString region() const;
    void setRegion(const QString& region);

    /// The document's saved keys. `lastExpiry` and `lastRecall` are the
    /// facade's, published but not saved.
    QJsonObject toJson() const;

    /// An unknown schema or a bad shape returns false with no change.
    bool fromJson(const QJsonObject& document);

    /// The band `key` names as bandKeyName writes it, shortwave bands included.
    /// Nullopt for anything else.
    static std::optional<Band> bandFromKey(const QString& key);

    /// A frequency as refusals and reports show it: 7.185 or 7.185.250.
    static QString displayFrequency(qint64 frequencyHz);

private:
    struct BandState {
        QStringList order;
        BandStackReturnMode returnMode{BandStackReturnMode::Current};
        QString specificId;
    };

    bool isDuplicate(Band band, qint64 frequencyHz, const QString& excludeId) const;
    bool isDuplicateInMode(Band band, qint64 frequencyHz, const QString& mode) const;
    BandStackTarget currentTarget(QChar slice, Band band) const;
    BandStackEntry addEntry(Band band, const BandStackSpot& spot, qint64 nowMs);
    void insertInOrder(const QString& id, Band band, qint64 frequencyHz);
    void eraseEntry(const QString& id);
    void addSeeds(const QList<BandStackEntry>& seeds);

    QMap<QString, BandStackEntry> m_entries;
    QMap<Band, BandState> m_bands;
    QMap<QChar, QMap<Band, QString>> m_places;
    BandStackOptions m_options;
    QString m_region;
};

} // namespace NereusSDR
