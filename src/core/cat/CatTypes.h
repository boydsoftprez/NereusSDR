//=================================================================
// CATParser.cs
//=================================================================
// Copyright (C) 2012  Bob Tracy
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
// You may contact the author via email at: k5kdn@arrl.net
//=================================================================
// Ported from Thetis Project Files/Source/Console/CAT/CATParser.cs
// (catalogue facts; independently implemented Qt loader).
// Modification history (NereusSDR):
// 2026-10-04 - C++20/Qt6 catalogue types and metadata loading by J.J. Boyd,
//              with AI-assisted transformation via Codex.
// Upstream parser dispatch is not ported here; catalogue facts and suffix
// categories follow CATParser.cs:475-590 [v2.10.3.15].

#pragma once
#include <QByteArray>
#include <QString>
#include <QtGlobal>
#include <optional>
namespace NereusSDR {
enum class CatVfo { Primary, Secondary };
enum class CatForm { Get, Set };
enum class CatResultKind { Payload, Silence, Wire, Error };
enum class CatOutcome { Faithful, Adapted, Inactive, SourceInert, Unavailable };
// From Thetis CAT/CATParser.cs:550-565 [v2.10.3.15]. Numeric permits the
// source optional sign and V/v digits grammar; semantic checks belong to handlers.
enum class CatSuffixKind { Numeric, Text, Equalizer, Guid };
enum class CatFormPrecedence { SetFirst, GetFirst };
struct CatBinding {
    int primarySliceId{-1};
    std::optional<int> secondarySliceId;
    quint64 primaryIncarnation{0};
    std::optional<quint64> secondaryIncarnation;
};
struct CatRequest { QByteArray code; QByteArray suffix; CatForm form; };
struct CatCommandResult { CatResultKind kind; QByteArray data; int verboseErrorCode{0}; };
struct CatValidation { std::optional<CatRequest> request; QByteArray error; int verboseErrorCode{0}; QByteArray errorCommand; };
struct CatSessionContext { quint64 sessionId; int channel; bool verboseErrors{false}; };
// From Thetis CAT/CATParser.cs:475-500 [v2.10.3.15]. Negative widths disable forms.
struct CatDescriptor {
    QByteArray code;
    bool active{false};
    int setWidth{-1};
    int getWidth{-1};
    int answerWidth{-1};
    CatSuffixKind suffixKind{CatSuffixKind::Numeric};
    CatOutcome outcome{CatOutcome::Unavailable};
    QString family;
    CatFormPrecedence formPrecedence{CatFormPrecedence::SetFirst};
};
} // namespace NereusSDR
