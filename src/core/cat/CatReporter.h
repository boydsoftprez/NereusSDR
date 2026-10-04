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

// Ported from Thetis Project Files/Source/Console/console.cs
// Modification history (NereusSDR):
// 2026-10-04 - Native event-loop CAT adaptation by J.J. Boyd (KG4VCF),
//              AI-assisted via OpenAI Codex.

#pragma once
#include "CatTypes.h"
#include <QObject>
#include <QPointer>
#include <QHash>
#include <QSet>
#include <QTimer>
#include <QElapsedTimer>
#include <functional>
#include <optional>
namespace NereusSDR {
class CatService; class RadioModel; class SliceModel;
class CatReporter : public QObject {
    Q_OBJECT
public:
    CatReporter(CatService&, RadioModel&);
    void setAiEnabled(bool);
    void flushPending();
    void reset();
    void sessionsChanged(int channel);
    void configurationChanged();
#ifdef NEREUS_BUILD_TESTS
    void setClockForTest(std::function<qint64()>);
#endif
    // From Thetis console.cs:53886 [v2.10.3.15].
    static constexpr qint64 kAiIntervalMs = 200;
    // From Thetis console.cs:7927 [v2.10.3.15].
    //MW0LGE_22a
    // [original inline comment from console.cs:7927]
    static constexpr int kFrequencyWidth = 11;
private:
    struct State {
        CatBinding binding;
        QByteArray lastMessage;
        QByteArray pending;
        std::optional<qint64> lastSentMs;
    };
    void watchSlice(SliceModel*);
    void markChanged(int sliceId, bool selection = false);
    void refreshChanges();
    void queue(int channel, const QByteArray& code, const CatBinding&, const QByteArray&);
    void scheduleTimer();
    bool eligible(quint64) const;
    bool bindingCurrent(int, const CatBinding&) const;
    bool messageCurrent(int, const CatBinding&, const QByteArray&) const;
    void broadcast(int, const CatBinding&, const QByteArray&);
    qint64 now() const;
    QPointer<CatService> m_service;
    QPointer<RadioModel> m_model;
    QTimer m_timer;
    QElapsedTimer m_elapsed;
    std::function<qint64()> m_clock;
    QHash<int, State> m_states;
    QSet<int> m_frequencyChanges;
    bool m_selectionChanged{false};
    bool m_refreshQueued{false};
    bool m_aiEnabled{false};
};
} // namespace NereusSDR
