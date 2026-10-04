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

#include "CatCommandRouter.h"
#include <QUuid>
#include <QRegularExpression>
#include <algorithm>
namespace NereusSDR {
namespace {
// From Thetis CAT/CATParser.cs:1590-1592 [v2.10.3.15].
constexpr int kFeatureNotAvailable = 7;
}
CatCommandRouter::CatCommandRouter()
{
    // From Thetis CAT/CATCommands.cs:2589-2613 [v2.10.3.15].
    //Provides verbose CAT error reporting
    // [original inline comment from CATCommands.cs:2589]
    // Adaptation: each calling session owns its verbose flag; never persist it.
    registerHandler("ZZEM", [](const CatRequest& request, CatSessionContext& session) {
        if (request.form == CatForm::Set && (request.suffix == "1" || request.suffix == "0")) {
            session.verboseErrors = request.suffix == "1";
            return CatCommandResult{CatResultKind::Silence, {}};
        }
        if (request.form == CatForm::Get) {
            return CatCommandResult{CatResultKind::Payload, session.verboseErrors ? "1" : "0"};
        }
        return CatCommandResult{CatResultKind::Error, "?;"};
    });
    // From Thetis CAT/CATCommands.cs:3099-3140 [v2.10.3.15].
    // Adds an id mainly used by tcpip cat ot direct messages back to a specific cat client, there is no get
    // TCPIPcatserver will use the response to add an id against the client
    // [original inline comment from CATCommands.cs:3099-3100]
    // Remove an id mainly used by tcpip cat ot direct messages back to a specific cat client, there is no get
    // TCPIPcatserver will use the response to remove an id from the client
    // [original inline comment from CATCommands.cs:3119-3120]
    const Handler guidHandler = [](const CatRequest& request, CatSessionContext&) {
        static const QRegularExpression kGuidPattern(QStringLiteral(
            "^[0-9A-Fa-f]{8}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{12}$"));
        if (request.form != CatForm::Set || !kGuidPattern.match(QString::fromLatin1(request.suffix)).hasMatch()) {
            return CatCommandResult{CatResultKind::Error, "?;"};
        }
        const QUuid guid(QString::fromLatin1(request.suffix));
        // Approved correction: upstream serial parser duplicates prefix/suffix.
        // Runtime TCP registration/removal is a transport responsibility (Task 8).
        return CatCommandResult{CatResultKind::Wire,
            request.code + guid.toString(QUuid::WithoutBraces).toLatin1().toLower() + ';'};
    };
    registerHandler("ZZGA", guidHandler);
    registerHandler("ZZGR", guidHandler);
}
bool CatCommandRouter::registerHandler(const QByteArray& code, Handler handler)
{
    const CatDescriptor* descriptor = m_catalog.find(code);
    if (!descriptor || !descriptor->active || !handler || m_handlers.contains(code)) { return false; }
    m_handlers.insert(code, std::move(handler));
    return true;
}
CatCommandResult CatCommandRouter::execute(const CatRequest& request, CatSessionContext& session)
{
    const CatDescriptor* descriptor = m_catalog.find(request.code);
    if (!descriptor || !descriptor->active) { return {CatResultKind::Error, "?;"}; }
    const auto handler = m_handlers.constFind(request.code);
    if (handler == m_handlers.cend()) {
        // No family exists yet: explicit unavailable result, never success.
        return {CatResultKind::Error, "?;", kFeatureNotAvailable};
    }
    return (*handler)(request, session);
}
QList<QByteArray> CatCommandRouter::registeredCodes() const
{
    QList<QByteArray> codes = m_handlers.keys();
    std::sort(codes.begin(), codes.end());
    return codes;
}
} // namespace NereusSDR
