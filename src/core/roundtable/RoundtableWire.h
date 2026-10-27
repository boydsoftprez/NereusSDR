#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// src/core/roundtable/RoundtableWire.h  (NereusSDR)
// =================================================================
//
// The Roundtable wire, version 1 (docs/architecture/2026-10-23-roundtable-
// v1.md sections 4 and 5): the four message tables, a strict decoder for
// each direction, a compact encoder, the station's message builders, and
// the identity values (the sign-in transcript, the Roundtable id) and the
// callsign, grid, mode and band rules a Core applies before it sends.
//
// The decoder reads a frame by section 5.1's rules, as the server's
// protocol.py does: a top-level object with a string `type`, no duplicate
// key at any depth, no NaN or Infinity, a whole number written with no
// fraction or exponent, a `dbm` with no exponent, no lone surrogate in a
// listed key. It returns the message with only the keys its kind lists, at
// every depth; a key a kind does not list is ignored. QJsonDocument cannot
// do this (it merges duplicate keys and forgets how a number was written),
// so the parser here is the wire's own.
//
// The Core decodes all four directions: what the server sends a station
// and a live reader, which it receives, and what a station and a live
// reader send, which it checks before sending (encode() then decode()), so
// it never sends a message the server would close the connection for.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-24: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code (Roundtable plan Task 10, R-RT-1 Core runner).
//   2026-10-24: placeSlice() and familyWidthHz(), shared by the Core's
//               state and its live reader (Roundtable plan Task 11 fix
//               round). J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
//   2026-10-26: kMinDbm and kMaxDbm, the reading range the Core's client
//               and the desktop panel share. J.J. Boyd (KG4VCF),
//               AI-assisted via Claude Code.
// =================================================================

#include "core/NereusCoreExport.h"

#include <QByteArray>
#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>
#include <QUrl>

namespace NereusSDR {

/// One slice as `presence` lists it (wire spec 5.3.2). `mode` is a wire
/// mode (`USB`, `CWU`, `FDVL`, ...).
struct RoundtableSliceState {
    int id = 0;
    qint64 hz = 0;
    QString mode;
    int filterHz = 0;
    bool keyed = false;
};

namespace RoundtableWire {

/// The protocol version this build speaks (5.3.1).
inline constexpr int kVersion = 1;
/// Section 2.2: the most a station sends in one message (the server's
/// inbound cap) and the most it accepts from the server.
inline constexpr qsizetype kMaxOutgoingBytes = 16384;
inline constexpr qsizetype kMaxIncomingBytes = 1048576;
/// Section 5.3.2: slices in one presence.
inline constexpr int kMaxSlices = 4;
/// Section 5.3.13: the words deleteArchive confirms with.
inline constexpr const char* kDeleteArchiveWords = "delete my archive";
/// Section 5.2 (`dbm`, used by 5.3.3 and 6.5): what a reading's dbm may
/// be, -160 to 94 inclusive. Outside it a slice has no reading (a cleared
/// slice holds -400).
inline constexpr double kMinDbm = -160.0;
inline constexpr double kMaxDbm = 94.0;

/// Who sends a message and who receives it (5.3, 5.4, 5.5).
enum class Direction { StationToServer, LiveToServer, ServerToStation, ServerToLive };

struct Decoded {
    bool ok = false;
    /// The kind (`type`), when the frame had a string one.
    QString type;
    /// The message with only the keys its kind lists, `type` first.
    QJsonObject message;
    /// Why it did not decode (for the log; never shown to an operator).
    QString error;
};

/// Decodes one text frame travelling in `direction`.
NEREUS_CORE_EXPORT Decoded decode(const QString& text, Direction direction);

/// Compact JSON: `type` first, non-ASCII written as UTF-8, a whole number
/// with no fraction, any other number with no exponent.
NEREUS_CORE_EXPORT QByteArray encode(const QJsonObject& message);

/// The exact source text of `key`'s value in the JSON object `text`, as it
/// is written there (the conformance runner reads fixtures through it, so
/// a fixture's `14212000.0` reaches the decoder as written). Empty when
/// `text` is not an object holding `key`.
NEREUS_CORE_EXPORT QString memberSourceText(const QString& text, const QString& key);

// ---- Station messages (5.3). Each is checked by decode() before sending.

NEREUS_CORE_EXPORT QJsonObject hello(const QString& software, const QString& softwareVersion,
                                     const QByteArray& spki, const QByteArray& signature);
NEREUS_CORE_EXPORT QJsonObject helloWithKey(const QString& software,
                                            const QString& softwareVersion,
                                            const QByteArray& websiteKey);
/// An empty `callsign` or `grid` is sent as null.
NEREUS_CORE_EXPORT QJsonObject presence(const QString& callsign, const QString& grid,
                                        bool atRadio,
                                        const QList<RoundtableSliceState>& slices);
NEREUS_CORE_EXPORT QJsonObject reading(int sliceId, double dbm, qint64 unixMs, bool present);
NEREUS_CORE_EXPORT QJsonObject post(const QString& roomId, const QString& text,
                                    const QString& clientId);
NEREUS_CORE_EXPORT QJsonObject deleteMessage(qint64 messageId);
NEREUS_CORE_EXPORT QJsonObject name(qint64 turnId, const QString& callsign);
/// An empty `reason` is sent as null.
NEREUS_CORE_EXPORT QJsonObject reportMessage(qint64 messageId, const QString& reason);
NEREUS_CORE_EXPORT QJsonObject reportTurn(qint64 turnId, const QString& reason);
NEREUS_CORE_EXPORT QJsonObject reportStation(const QString& roomId, const QString& ref,
                                             const QString& reason);
NEREUS_CORE_EXPORT QJsonObject read(const QString& roomId);
NEREUS_CORE_EXPORT QJsonObject unread(const QString& roomId);
/// An empty `roomId` asks for the band list (`room` null).
NEREUS_CORE_EXPORT QJsonObject resync(const QString& roomId);
NEREUS_CORE_EXPORT QJsonObject mentionsSeen(qint64 unixMs);
NEREUS_CORE_EXPORT QJsonObject weblink();
NEREUS_CORE_EXPORT QJsonObject deleteArchive(const QString& confirm);
NEREUS_CORE_EXPORT QJsonObject hidden();

// ---- Live reader messages (5.5).

/// `band` is a band of 6.2, `other` or `all`.
NEREUS_CORE_EXPORT QJsonObject watchBand(const QString& band);
NEREUS_CORE_EXPORT QJsonObject watch(const QString& roomId);
NEREUS_CORE_EXPORT QJsonObject unwatch(const QString& roomId);

// ---- Identity values (4.1, 4.3).

/// "NereusSDR roundtable hello v1\n" || host || "\n" || nonce (32 raw bytes).
NEREUS_CORE_EXPORT QByteArray helloTranscript(const QString& host, const QByteArray& nonce);
/// The host a station signs (4.3): the URL's host, lower case, no port, no
/// trailing dot.
NEREUS_CORE_EXPORT QString signingHost(const QUrl& url);
/// base32 lower case of SHA-256("NereusSDR roundtable id v1\n" || spki), its
/// first 26 characters.
NEREUS_CORE_EXPORT QString roundtableId(const QByteArray& spki);
/// Strict base64url (4.1): the alphabet, no padding, a length that is not 1
/// mod 4 and the unused low bits zero. Sets *ok.
NEREUS_CORE_EXPORT QByteArray fromBase64Url(const QString& text, bool* ok);
NEREUS_CORE_EXPORT QString toBase64Url(const QByteArray& bytes);

// ---- Values (4.5, 6.1, 6.2).

/// 3 to 16 of A-Z a-z 0-9 /, a letter and a digit, no leading, trailing or
/// doubled /.
NEREUS_CORE_EXPORT bool isCallsign(const QString& text);
/// The longest `/`-separated part, the first of them on a tie, upper case.
NEREUS_CORE_EXPORT QString baseCallsign(const QString& callsign);
/// A 4 or 6 character Maidenhead locator, in any case.
NEREUS_CORE_EXPORT bool isGrid(const QString& text);
/// What a Core sends for `text` (4.5): an 8-character grid cut to 6, then
/// the grid with its field upper case and subsquare lower case, or empty
/// (sent as null) when it is not a grid.
NEREUS_CORE_EXPORT QString gridToSend(const QString& text);
/// The family of a wire mode (6.1), empty for none.
NEREUS_CORE_EXPORT QString familyOfMode(const QString& mode);
/// The band holding `hz` (6.2), or `other`.
NEREUS_CORE_EXPORT QString bandOf(qint64 hz);
/// A room in the band list, as far as placing a slice needs.
struct ListedRoom {
    QString family;
    qint64 hz = 0;
};
/// How far either side of a room's center a slice of `family` may be and
/// still be in that room (6.1).
NEREUS_CORE_EXPORT qint64 familyWidthHz(const QString& family);
/// The listed room `slice` is in (6.3), or empty for none: the room of its
/// family whose center is nearest, within the family's width, the lower
/// center on a tie; it stays in `previous` while its family is unchanged
/// and it is still within the width.
NEREUS_CORE_EXPORT QString placeSlice(const RoundtableSliceState& slice, const QString& previous,
                                      const QHash<QString, ListedRoom>& rooms);
/// The base callsign (4.5) of every word in a message's text (6.10): the
/// runs of A-Z a-z 0-9 / with an optional leading @, upper case, each once.
/// A station is mentioned when its own base callsign is among them.
NEREUS_CORE_EXPORT QStringList mentionedBaseCallsigns(const QString& text);

} // namespace RoundtableWire
} // namespace NereusSDR
