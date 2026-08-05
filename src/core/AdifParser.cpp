// SPDX-License-Identifier: GPL-3.0-or-later
//
// NereusSDR - AdifParser: ADIF (.adi / .adif) amateur-radio log parser.
//
// Ported from AetherSDR src/core/AdifParser.cpp [@0cd4559].
// AetherSDR is (C) its contributors and is licensed GPL-3.0-or-later
// (see https://github.com/ten9876/AetherSDR/blob/main/LICENSE).
//
// Modification history (NereusSDR):
//   2026-05-10  J.J. Boyd / KG4VCF  Phase 3J-2 Task C2. Initial port.
//                                    AetherSDR's "AetherSDR" namespace
//                                    becomes "NereusSDR". Public API
//                                    (parseFile, parseFileAsync) and
//                                    internal helpers (extractField,
//                                    normaliseMode, freqToBand,
//                                    parse) follow upstream
//                                    byte-for-byte: header skip on
//                                    <EOH>, <EOR>-delimited record
//                                    iteration, ADIF tag regex
//                                    `<FIELDNAME:length>value` with
//                                    optional `:TYPE` segment, BAND
//                                    fallback to FREQ -> freqToBand,
//                                    submode-aware mode group
//                                    classification (FT8 / FT4 /
//                                    JS8 / JT65 / JT9 / WSPR /
//                                    PSK31 / PSK63 / RTTY -> DATA),
//                                    3-attempt open retry with
//                                    500ms backoff on async path.
//                                    NereusSDR addition:
//                                    parseBytesForTest() public
//                                    test seam (precedent: B1-B5)
//                                    that delegates to the private
//                                    parse() so tests can exercise
//                                    the parser against an
//                                    in-memory buffer without a
//                                    QFile round-trip.
//                                    AI tooling: Anthropic Claude
//                                    Code.

#include "AdifParser.h"

#include <QFile>
#include <QRegularExpression>
#include <QThread>
#include <QtAlgorithms>

namespace NereusSDR {

// From AetherSDR src/core/AdifParser.cpp:10-29 [@0cd4559]
//
// ---------------------------------------------------------------------------
// Static helpers
// ---------------------------------------------------------------------------

// Extract the value of a named ADIF field from a block of text.
// ADIF field format: <FIELDNAME:length>value  (case insensitive)
static QString extractField(const QString& block, const QString& fieldName)
{
    // e.g. <CALL:6>G3ABC  or <CALL:6:S>G3ABC
    // NOTE: do NOT make this regex static — the pattern depends on fieldName.
    const QRegularExpression re(
        R"(<)" + QRegularExpression::escape(fieldName) + R"((?::\d+(?::[A-Z])?)?:(\d+)>)",
        QRegularExpression::CaseInsensitiveOption);
    auto m = re.match(block);
    if (!m.hasMatch()) return {};
    int len = m.captured(1).toInt();
    int start = m.capturedEnd(0);
    if (start + len > block.length()) return {};
    return block.mid(start, len);
}

// From AetherSDR src/core/AdifParser.cpp:31-54 [@0cd4559]
QString AdifParser::normaliseMode(const QString& mode, const QString& submode)
{
    // Check submode first (handles MFSK/FT8, MFSK/FT4, etc.)
    const QString sub = submode.toUpper();
    if (!sub.isEmpty()) {
        if (sub == "FT8" || sub == "FT4" || sub == "JS8" ||
            sub == "JT65" || sub == "JT9" || sub == "WSPR" ||
            sub == "PSK31" || sub == "PSK63" || sub == "RTTY")
            return "DATA";
    }

    const QString m = mode.toUpper();
    if (m == "CW")   return "CW";
    if (m == "SSB" || m == "USB" || m == "LSB" || m == "AM" || m == "FM")
        return "PHONE";
    if (m == "FT8"  || m == "FT4"  || m == "RTTY" || m == "PSK31" ||
        m == "PSK63" || m == "WSPR" || m == "JT65" || m == "JT9"  ||
        m == "JS8"   || m == "MFSK" || m == "OLIVIA" || m == "CONTESTIA" ||
        m == "SSTV"  || m == "PACKET" || m == "HELL"  || m == "ATV")
        return "DATA";
    // Unknown — treat as DATA
    if (!m.isEmpty()) return "DATA";
    return "PHONE";  // default
}

// From AetherSDR src/core/AdifParser.cpp:56-73 [@0cd4559]
QString AdifParser::freqToBand(double mhz)
{
    if (mhz >= 1.8   && mhz < 2.0)   return "160m";
    if (mhz >= 3.5   && mhz < 4.0)   return "80m";
    if (mhz >= 5.0   && mhz < 5.6)   return "60m";
    if (mhz >= 7.0   && mhz < 7.3)   return "40m";
    if (mhz >= 10.1  && mhz < 10.15) return "30m";
    if (mhz >= 14.0  && mhz < 14.35) return "20m";
    if (mhz >= 18.068&& mhz < 18.168)return "17m";
    if (mhz >= 21.0  && mhz < 21.45) return "15m";
    if (mhz >= 24.89 && mhz < 24.99) return "12m";
    if (mhz >= 28.0  && mhz < 29.7)  return "10m";
    if (mhz >= 50.0  && mhz < 54.0)  return "6m";
    if (mhz >= 70.0  && mhz < 70.5)  return "4m";
    if (mhz >= 144.0 && mhz < 148.0) return "2m";
    if (mhz >= 430.0 && mhz < 440.0) return "70cm";
    return {};
}

// From AetherSDR src/core/AdifParser.cpp:75-138 [@0cd4559]
//
// ---------------------------------------------------------------------------
// Core parser
// ---------------------------------------------------------------------------

QVector<QsoRecord> AdifParser::parse(const QByteArray& data)
{
    QVector<QsoRecord> records;
    const QString text = QString::fromUtf8(data);

    // Skip ADIF header section (everything before <EOH>)
    int bodyStart = 0;
    int eoh = text.indexOf("<EOH>", 0, Qt::CaseInsensitive);
    if (eoh != -1)
        bodyStart = eoh + 5;

    // Split on <EOR> record separators
    static const QRegularExpression eorRe("<EOR>", QRegularExpression::CaseInsensitiveOption);
    int pos = bodyStart;
    QRegularExpressionMatchIterator it = eorRe.globalMatch(text, bodyStart);

    while (it.hasNext()) {
        auto eorMatch = it.next();
        const QString block = text.mid(pos, eorMatch.capturedStart() - pos);
        pos = eorMatch.capturedEnd();

        if (block.trimmed().isEmpty()) continue;

        QsoRecord rec;
        rec.callsign = extractField(block, "CALL").trimmed().toUpper();
        if (rec.callsign.isEmpty()) continue;

        // Band: prefer explicit <BAND> field, fall back to <FREQ> → freqToBand.
        // Some loggers export bare numbers ("10", "20") instead of ADIF-standard
        // labels ("10m", "20m") — normalise those here.
        rec.band = extractField(block, "BAND").trimmed().toLower();
        if (!rec.band.isEmpty() && !rec.band.endsWith('m') && !rec.band.endsWith("cm")) {
            static const QHash<QString,QString> bandMap = {
                {"160","160m"},{"80","80m"},{"60","60m"},{"40","40m"},
                {"30","30m"},{"20","20m"},{"17","17m"},{"15","15m"},
                {"12","12m"},{"10","10m"},{"6","6m"},{"4","4m"},
                {"2","2m"},{"70","70cm"},
            };
            const QString mapped = bandMap.value(rec.band);
            if (!mapped.isEmpty()) rec.band = mapped;
        }
        if (rec.band.isEmpty()) {
            const QString freqStr = extractField(block, "FREQ").trimmed();
            if (!freqStr.isEmpty()) {
                bool ok = false;
                double mhz = freqStr.toDouble(&ok);
                if (ok) rec.band = freqToBand(mhz);
            }
        }

        // Mode
        const QString mode    = extractField(block, "MODE").trimmed();
        const QString submode = extractField(block, "SUBMODE").trimmed();
        rec.modeGroup = normaliseMode(mode, submode);

        records.append(rec);
    }

    return records;
}

// From AetherSDR src/core/AdifParser.cpp:140-150 [@0cd4559]
//
// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

QVector<QsoRecord> AdifParser::parseFile(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return parse(f.readAll());
}

// NereusSDR-only: pass-through test seam delegating to private parse().
// Precedent: B1 / B2 / B3 / B5 introduced equivalent *ForTest() seams
// against their respective parsers.
QVector<QsoRecord> AdifParser::parseBytesForTest(const QByteArray& data)
{
    return parse(data);
}

// From AetherSDR src/core/AdifParser.cpp:152-173 [@0cd4559]
void AdifParser::parseFileAsync(const QString& path)
{
    // If the file is held open by an external logger (common on Windows),
    // QFile::open() will fail.  Retry a few times before giving up so that
    // a brief write-lock doesn't wipe every spot colour.
    constexpr int kMaxAttempts  = 3;
    constexpr int kRetryDelayMs = 500;

    for (int attempt = 0; attempt < kMaxAttempts; ++attempt) {
        QFile f(path);
        if (f.open(QIODevice::ReadOnly)) {
            emit finished(parse(f.readAll()));
            return;
        }
        if (attempt + 1 < kMaxAttempts)
            QThread::msleep(kRetryDelayMs);
    }

    // All attempts failed — tell the caller so it can keep its existing
    // worked status rather than replacing it with an empty result set.
    emit openFailed(path);
}

} // namespace NereusSDR
