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
#include "CatTypes.h"
#include <QList>
namespace NereusSDR {
class CatCommandCatalog {
public:
    explicit CatCommandCatalog(const QString& path = QStringLiteral(":/cat/CATStructs.xml"));
    const QList<CatDescriptor>& descriptors() const { return m_descriptors; }
    const CatDescriptor* find(const QByteArray& code) const;
    qsizetype maximumRequestBytes() const { return m_maximumRequestBytes; }
    bool isValid() const { return m_valid; }
    QString errorString() const { return m_error; }
private:
    bool load(const QString& path);
    QList<CatDescriptor> m_descriptors;
    qsizetype m_maximumRequestBytes{0};
    bool m_valid{false};
    QString m_error;
};
} // namespace NereusSDR
