//=================================================================
// CATCommands.cs
//=================================================================
// Copyright (C) 2005  Bob Tracy
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
// Continual modifications Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
/*
Modifications to support the Behringer Midi controllers
by Chris Codella, W2PA, April 2017.  Indicated by //-W2PA comment lines.
Added extended CAT commands for APF funtions - May 2017.
*/
//=================================================================

// Ported from Thetis Project Files/Source/Console/CAT/CATCommands.cs
// Modification history (NereusSDR):
// 2026-10-04 - C++20/Qt6 parser/session dispatch by J.J. Boyd,
//              with AI-assisted transformation via Codex.

#pragma once
#include "CatCommandCatalog.h"
#include <QHash>
#include <functional>
namespace NereusSDR {
class CatCommandRouter {
public:
    using Handler = std::function<CatCommandResult(const CatRequest&, CatSessionContext&)>;
    CatCommandRouter();
    bool registerHandler(const QByteArray& code, Handler handler);
    CatCommandResult execute(const CatRequest& request, CatSessionContext& session);
    QList<QByteArray> registeredCodes() const;
private:
    CatCommandCatalog m_catalog;
    QHash<QByteArray, Handler> m_handlers;
};
} // namespace NereusSDR
