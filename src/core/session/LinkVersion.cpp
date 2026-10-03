// =================================================================
// src/core/session/LinkVersion.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original.
//
// See LinkVersion.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Task 4 (R-IOS-01): link
//                                    majors, their agreement and the
//                                    refusal wording. AI-assisted
//                                    transformation via Anthropic Claude
//                                    Code.
//   2026-09-24  J.J. Boyd / KG4VCF  R3 completion carry, review I1
//                                    (R-R3-38, R-IOS-01): the refusal
//                                    wording moves to SessionEndReasons.
//                                    AI-assisted transformation via
//                                    Anthropic Claude Code.
// =================================================================

#include "core/session/LinkVersion.h"

#include <QStringList>

#include <algorithm>

namespace NereusSDR::LinkVersion {

QList<quint16> supportedMajors()
{
    return QList<quint16>(kSupportedSessionMajors.cbegin(), kSupportedSessionMajors.cend());
}

std::optional<quint16> agreeMajor(QList<quint16> ours, QList<quint16> theirs)
{
    std::optional<quint16> agreed;
    for (const quint16 major : ours) {
        if (theirs.contains(major) && (!agreed || major > *agreed)) {
            agreed = major;
        }
    }
    return agreed;
}

QList<quint16> parseMajorList(const QString& text, QString* error)
{
    QList<quint16> majors;
    const QStringList parts = text.split(QLatin1Char(','));
    for (const QString& part : parts) {
        bool ok = false;
        const uint value = part.trimmed().toUInt(&ok);
        if (!ok || value < 1 || value > 65535) {
            if (error) {
                *error = QStringLiteral("Give the link versions as whole numbers from 1 to "
                                        "65535, separated by commas, for example 1,2.");
            }
            return {};
        }
        if (!majors.contains(quint16(value))) {
            majors.append(quint16(value));
        }
    }
    std::sort(majors.begin(), majors.end());
    return majors;
}

bool testLinkMajorsAllowed()
{
#ifdef QT_NO_DEBUG
    return false;
#else
    return true;
#endif
}

QList<quint16> resolveTestLinkMajors(bool optionSet, const QString& value, bool allowed,
                                     QString* error)
{
    if (!optionSet) {
        return supportedMajors();
    }
    if (!allowed) {
        if (error) {
            *error = QStringLiteral("--test-link-majors works only in a debug build of "
                                    "nereusd.");
        }
        return {};
    }
    return parseMajorList(value, error);
}

} // namespace NereusSDR::LinkVersion
