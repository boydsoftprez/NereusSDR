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
// Modification history (NereusSDR):
// 2026-10-04 - C++20/Qt6 parser/session dispatch by J.J. Boyd,
//              with AI-assisted transformation via Codex.

#pragma once
#include "CatCommandCatalog.h"
namespace NereusSDR {
class CatParser {
public:
    CatParser() = default;
    explicit CatParser(const CatCommandCatalog& catalog) : m_catalog(catalog) {}
    CatValidation validate(const QByteArray& frame) const;
    QByteArray formatValidationError(const CatValidation& validation, const CatSessionContext& session) const;
    QByteArray format(const CatDescriptor& descriptor, const CatRequest& request,
                      const CatCommandResult& result, const CatSessionContext& session) const;
private:
    QByteArray formatError(const QByteArray& error, int code, const QByteArray& command,
                           const CatSessionContext& session) const;
    CatCommandCatalog m_catalog;
};
} // namespace NereusSDR
