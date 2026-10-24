// no-port-check: NereusSDR-original.
// =================================================================
// src/core/roundtable/RoundtableWire.cpp  (NereusSDR)
// =================================================================
//
// See RoundtableWire.h. The tables below are the wire spec's sections 5.2
// to 5.5, in the order the server's protocol.py lists them, so the two
// readings of the spec can be compared line by line.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-24: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code (Roundtable plan Task 10, R-RT-1 Core runner).
// =================================================================

#include "core/roundtable/RoundtableWire.h"

#include <QCryptographicHash>
#include <QHash>
#include <QJsonArray>
#include <QJsonValue>
#include <QLocale>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>
#include <cmath>
#include <functional>
#include <utility>
#include <vector>

namespace NereusSDR::RoundtableWire {

namespace {

constexpr qint64 kMaxSafe = 9007199254740991LL;
constexpr qint64 kMaxHz = 99999999999LL;
constexpr int kMaxDepth = 64;
constexpr int kSummaryTextCodePoints = 80;

// ----------------------------------------------------------------- parse

// One JSON value as written: how a number was written matters (5.1).
struct Node {
    enum class Kind { Null, Bool, Whole, Fraction, Exponent, String, Array, Object };
    Kind kind = Kind::Null;
    bool boolean = false;
    // A whole number that fits qint64 (anything longer is out of every
    // range the tables use).
    bool wholeFits = false;
    qint64 whole = 0;
    double number = 0.0;
    QString text;
    // False when the string holds a lone surrogate (5.1).
    bool scalarText = true;
    std::vector<Node> items;
    std::vector<QString> keys;
    std::vector<Node> values;
    int start = 0;
    int end = 0;

    const Node* member(const QString& key) const
    {
        for (size_t i = 0; i < keys.size(); ++i) {
            if (keys[i] == key) {
                return &values[i];
            }
        }
        return nullptr;
    }
};

class Parser {
public:
    explicit Parser(const QString& text) : m_s(text) {}

    bool parseDocument(Node& out, QString& why)
    {
        skipSpace();
        if (!parseValue(out, 0)) {
            why = m_error.isEmpty() ? QStringLiteral("not JSON") : m_error;
            return false;
        }
        skipSpace();
        if (m_i != m_s.size()) {
            why = QStringLiteral("not JSON: text after the value");
            return false;
        }
        return true;
    }

private:
    bool fail(const QString& what)
    {
        if (m_error.isEmpty()) {
            m_error = what;
        }
        return false;
    }

    void skipSpace()
    {
        while (m_i < m_s.size()) {
            const QChar c = m_s.at(m_i);
            if (c == u' ' || c == u'\t' || c == u'\n' || c == u'\r') {
                ++m_i;
            } else {
                break;
            }
        }
    }

    bool literal(const char* word)
    {
        const QLatin1String w(word);
        if (m_s.mid(m_i, w.size()) == w) {
            m_i += static_cast<int>(w.size());
            return true;
        }
        return false;
    }

    bool parseValue(Node& out, int depth)
    {
        if (depth > kMaxDepth) {
            return fail(QStringLiteral("not JSON: nested too deep"));
        }
        if (m_i >= m_s.size()) {
            return fail(QStringLiteral("not JSON: ends early"));
        }
        out.start = m_i;
        const QChar c = m_s.at(m_i);
        bool ok = false;
        if (c == u'{') {
            ok = parseObject(out, depth);
        } else if (c == u'[') {
            ok = parseArray(out, depth);
        } else if (c == u'"') {
            out.kind = Node::Kind::String;
            ok = parseString(out.text, out.scalarText);
        } else if (c == u'-' || (c >= u'0' && c <= u'9')) {
            ok = parseNumber(out);
        } else if (literal("true")) {
            out.kind = Node::Kind::Bool;
            out.boolean = true;
            ok = true;
        } else if (literal("false")) {
            out.kind = Node::Kind::Bool;
            out.boolean = false;
            ok = true;
        } else if (literal("null")) {
            out.kind = Node::Kind::Null;
            ok = true;
        } else {
            // NaN, Infinity and anything else.
            ok = fail(QStringLiteral("not JSON"));
        }
        out.end = m_i;
        return ok;
    }

    bool parseObject(Node& out, int depth)
    {
        out.kind = Node::Kind::Object;
        ++m_i;
        skipSpace();
        if (m_i < m_s.size() && m_s.at(m_i) == u'}') {
            ++m_i;
            return true;
        }
        while (true) {
            skipSpace();
            if (m_i >= m_s.size() || m_s.at(m_i) != u'"') {
                return fail(QStringLiteral("not JSON: a key is not a string"));
            }
            QString key;
            bool scalar = true;
            if (!parseString(key, scalar)) {
                return false;
            }
            for (const QString& existing : out.keys) {
                if (existing == key) {
                    return fail(QStringLiteral("the key %1 appears twice").arg(key));
                }
            }
            skipSpace();
            if (m_i >= m_s.size() || m_s.at(m_i) != u':') {
                return fail(QStringLiteral("not JSON: no colon"));
            }
            ++m_i;
            skipSpace();
            Node value;
            if (!parseValue(value, depth + 1)) {
                return false;
            }
            out.keys.push_back(key);
            out.values.push_back(std::move(value));
            skipSpace();
            if (m_i >= m_s.size()) {
                return fail(QStringLiteral("not JSON: ends early"));
            }
            if (m_s.at(m_i) == u',') {
                ++m_i;
                continue;
            }
            if (m_s.at(m_i) == u'}') {
                ++m_i;
                return true;
            }
            return fail(QStringLiteral("not JSON: no comma"));
        }
    }

    bool parseArray(Node& out, int depth)
    {
        out.kind = Node::Kind::Array;
        ++m_i;
        skipSpace();
        if (m_i < m_s.size() && m_s.at(m_i) == u']') {
            ++m_i;
            return true;
        }
        while (true) {
            skipSpace();
            Node value;
            if (!parseValue(value, depth + 1)) {
                return false;
            }
            out.items.push_back(std::move(value));
            skipSpace();
            if (m_i >= m_s.size()) {
                return fail(QStringLiteral("not JSON: ends early"));
            }
            if (m_s.at(m_i) == u',') {
                ++m_i;
                continue;
            }
            if (m_s.at(m_i) == u']') {
                ++m_i;
                return true;
            }
            return fail(QStringLiteral("not JSON: no comma"));
        }
    }

    static int hexValue(QChar c)
    {
        const ushort u = c.unicode();
        if (u >= '0' && u <= '9') {
            return u - '0';
        }
        if (u >= 'a' && u <= 'f') {
            return u - 'a' + 10;
        }
        if (u >= 'A' && u <= 'F') {
            return u - 'A' + 10;
        }
        return -1;
    }

    bool parseHex4(ushort& out)
    {
        if (m_i + 4 > m_s.size()) {
            return fail(QStringLiteral("not JSON: a short escape"));
        }
        int value = 0;
        for (int k = 0; k < 4; ++k) {
            const int h = hexValue(m_s.at(m_i + k));
            if (h < 0) {
                return fail(QStringLiteral("not JSON: a bad escape"));
            }
            value = value * 16 + h;
        }
        m_i += 4;
        out = static_cast<ushort>(value);
        return true;
    }

    bool parseString(QString& out, bool& scalar)
    {
        ++m_i;  // the opening quote
        scalar = true;
        // UTF-16 units as written or escaped; checked for pairing after.
        QString units;
        while (true) {
            if (m_i >= m_s.size()) {
                return fail(QStringLiteral("not JSON: an open string"));
            }
            const QChar c = m_s.at(m_i);
            if (c == u'"') {
                ++m_i;
                break;
            }
            if (c.unicode() < 0x20) {
                return fail(QStringLiteral("not JSON: a control character in a string"));
            }
            if (c != u'\\') {
                units.append(c);
                ++m_i;
                continue;
            }
            ++m_i;
            if (m_i >= m_s.size()) {
                return fail(QStringLiteral("not JSON: an open string"));
            }
            const QChar e = m_s.at(m_i);
            ++m_i;
            switch (e.unicode()) {
            case '"': units.append(u'"'); break;
            case '\\': units.append(u'\\'); break;
            case '/': units.append(u'/'); break;
            case 'b': units.append(QChar(0x08)); break;
            case 'f': units.append(QChar(0x0C)); break;
            case 'n': units.append(u'\n'); break;
            case 'r': units.append(u'\r'); break;
            case 't': units.append(u'\t'); break;
            case 'u': {
                ushort unit = 0;
                if (!parseHex4(unit)) {
                    return false;
                }
                units.append(QChar(unit));
                break;
            }
            default:
                return fail(QStringLiteral("not JSON: a bad escape"));
            }
        }
        // A high surrogate must be followed by a low one, and a low one
        // preceded by a high one; otherwise the string is not Unicode
        // scalar values and cannot travel as UTF-8 (5.1).
        for (int k = 0; k < units.size(); ++k) {
            const QChar u = units.at(k);
            if (u.isHighSurrogate()) {
                if (k + 1 < units.size() && units.at(k + 1).isLowSurrogate()) {
                    ++k;
                } else {
                    scalar = false;
                }
            } else if (u.isLowSurrogate()) {
                scalar = false;
            }
        }
        out = units;
        return true;
    }

    bool parseNumber(Node& out)
    {
        const int begin = m_i;
        if (m_s.at(m_i) == u'-') {
            ++m_i;
        }
        auto isDigit = [this](int at) {
            return at < m_s.size() && m_s.at(at) >= u'0' && m_s.at(at) <= u'9';
        };
        if (!isDigit(m_i)) {
            return fail(QStringLiteral("not JSON: a bad number"));
        }
        if (m_s.at(m_i) == u'0') {
            ++m_i;
        } else {
            while (isDigit(m_i)) {
                ++m_i;
            }
        }
        bool fraction = false;
        bool exponent = false;
        if (m_i < m_s.size() && m_s.at(m_i) == u'.') {
            ++m_i;
            if (!isDigit(m_i)) {
                return fail(QStringLiteral("not JSON: a bad number"));
            }
            while (isDigit(m_i)) {
                ++m_i;
            }
            fraction = true;
        }
        if (m_i < m_s.size() && (m_s.at(m_i) == u'e' || m_s.at(m_i) == u'E')) {
            ++m_i;
            if (m_i < m_s.size() && (m_s.at(m_i) == u'+' || m_s.at(m_i) == u'-')) {
                ++m_i;
            }
            if (!isDigit(m_i)) {
                return fail(QStringLiteral("not JSON: a bad number"));
            }
            while (isDigit(m_i)) {
                ++m_i;
            }
            exponent = true;
        }
        const QStringView written = QStringView(m_s).mid(begin, m_i - begin);
        if (exponent) {
            out.kind = Node::Kind::Exponent;
            out.number = written.toDouble();
        } else if (fraction) {
            out.kind = Node::Kind::Fraction;
            out.number = written.toDouble();
        } else {
            out.kind = Node::Kind::Whole;
            bool ok = false;
            const qint64 value = written.toLongLong(&ok);
            out.wholeFits = ok;
            out.whole = ok ? value : 0;
            out.number = written.toDouble();
        }
        return true;
    }

    const QString& m_s;
    int m_i = 0;
    QString m_error;
};

// ----------------------------------------------------------------- field kinds
//
// A field kind checks one value and gives its decoded form, objects with
// only the keys they list.

using Check = std::function<bool(const Node&, const QString&, QJsonValue&, QString&)>;
using Fields = std::vector<std::pair<QString, Check>>;
// The checks of keys that depend on each other: the raw object, the
// decoded keys so far, the path.
using Post = std::function<bool(const Node&, QJsonObject&, const QString&, QString&)>;

bool bad(QString& why, const QString& path, const QString& what)
{
    why = path + QStringLiteral(": ") + what;
    return false;
}

Check whole(qint64 lo, qint64 hi)
{
    return [lo, hi](const Node& v, const QString& path, QJsonValue& out, QString& why) {
        if (v.kind != Node::Kind::Whole || !v.wholeFits || v.whole < lo || v.whole > hi) {
            return bad(why, path, QStringLiteral("not a whole number from %1 to %2").arg(lo).arg(hi));
        }
        out = QJsonValue(v.whole);
        return true;
    };
}

Check number(double lo, double hi)
{
    return [lo, hi](const Node& v, const QString& path, QJsonValue& out, QString& why) {
        if (v.kind == Node::Kind::Whole) {
            if (!v.wholeFits || static_cast<double>(v.whole) < lo || static_cast<double>(v.whole) > hi) {
                return bad(why, path, QStringLiteral("not a number from %1 to %2").arg(lo).arg(hi));
            }
            out = QJsonValue(v.whole);
            return true;
        }
        if (v.kind != Node::Kind::Fraction || !std::isfinite(v.number) || v.number < lo || v.number > hi) {
            return bad(why, path, QStringLiteral("not a number from %1 to %2").arg(lo).arg(hi));
        }
        out = QJsonValue(v.number);
        return true;
    };
}

bool boolean(const Node& v, const QString& path, QJsonValue& out, QString& why)
{
    if (v.kind != Node::Kind::Bool) {
        return bad(why, path, QStringLiteral("not true or false"));
    }
    out = QJsonValue(v.boolean);
    return true;
}

using CharRule = std::function<bool(const QString&)>;

Check string(int lo, int hi, CharRule chars = {})
{
    return [lo, hi, chars](const Node& v, const QString& path, QJsonValue& out, QString& why) {
        if (v.kind != Node::Kind::String) {
            return bad(why, path, QStringLiteral("not a string"));
        }
        if (!v.scalarText) {
            return bad(why, path, QStringLiteral("a lone surrogate"));
        }
        const qsizetype n = v.text.toUtf8().size();
        if (n < lo || n > hi) {
            return bad(why, path, QStringLiteral("not a string of %1 to %2 bytes").arg(lo).arg(hi));
        }
        if (chars && !chars(v.text)) {
            return bad(why, path, QStringLiteral("a character not allowed here"));
        }
        out = QJsonValue(v.text);
        return true;
    };
}

Check pattern(const QString& regex, const QString& what)
{
    const QRegularExpression re(QStringLiteral("\\A(?:") + regex + QStringLiteral(")\\z"));
    return [re, what](const Node& v, const QString& path, QJsonValue& out, QString& why) {
        if (v.kind != Node::Kind::String || !v.scalarText || !re.match(v.text).hasMatch()) {
            return bad(why, path, QStringLiteral("not ") + what);
        }
        out = QJsonValue(v.text);
        return true;
    };
}

Check oneOf(const QStringList& values)
{
    return [values](const Node& v, const QString& path, QJsonValue& out, QString& why) {
        if (v.kind != Node::Kind::String || !values.contains(v.text)) {
            return bad(why, path, QStringLiteral("not one of ") + values.join(QStringLiteral(", ")));
        }
        out = QJsonValue(v.text);
        return true;
    };
}

Check base64Bytes(int n)
{
    return [n](const Node& v, const QString& path, QJsonValue& out, QString& why) {
        bool ok = false;
        if (v.kind == Node::Kind::String && v.scalarText) {
            const QByteArray bytes = fromBase64Url(v.text, &ok);
            ok = ok && bytes.size() == n;
        }
        if (!ok) {
            return bad(why, path, QStringLiteral("not strict base64url of %1 bytes").arg(n));
        }
        out = QJsonValue(v.text);
        return true;
    };
}

Check orNull(const Check& kind)
{
    return [kind](const Node& v, const QString& path, QJsonValue& out, QString& why) {
        if (v.kind == Node::Kind::Null) {
            out = QJsonValue(QJsonValue::Null);
            return true;
        }
        return kind(v, path, out, why);
    };
}

Check array(const Check& kind, int lo = 0, int hi = -1)
{
    return [kind, lo, hi](const Node& v, const QString& path, QJsonValue& out, QString& why) {
        if (v.kind != Node::Kind::Array) {
            return bad(why, path, QStringLiteral("not an array"));
        }
        const int n = static_cast<int>(v.items.size());
        if (n < lo || (hi >= 0 && n > hi)) {
            return bad(why, path, QStringLiteral("an array of the wrong length"));
        }
        QJsonArray items;
        for (int i = 0; i < n; ++i) {
            QJsonValue item;
            if (!kind(v.items[static_cast<size_t>(i)], path + QStringLiteral("[%1]").arg(i), item, why)) {
                return false;
            }
            items.append(item);
        }
        out = items;
        return true;
    };
}

bool takeFields(const Node& raw, const Fields& fields, const QString& path, QJsonObject& out,
                QString& why)
{
    for (const auto& [key, kind] : fields) {
        const Node* value = raw.member(key);
        if (value == nullptr) {
            return bad(why, path + key, QStringLiteral("missing"));
        }
        QJsonValue decoded;
        if (!kind(*value, path + key, decoded, why)) {
            return false;
        }
        out.insert(key, decoded);
    }
    return true;
}

Check object(Fields fields, Post post = {})
{
    return [fields = std::move(fields), post](const Node& v, const QString& path, QJsonValue& out,
                                              QString& why) {
        if (v.kind != Node::Kind::Object) {
            return bad(why, path, QStringLiteral("not an object"));
        }
        QJsonObject decoded;
        const QString prefix = path + QLatin1Char('.');
        if (!takeFields(v, fields, prefix, decoded, why)) {
            return false;
        }
        if (post && !post(v, decoded, prefix, why)) {
            return false;
        }
        out = decoded;
        return true;
    };
}

bool noControl(const QString& text)
{
    for (const QChar c : text) {
        // Unicode category Cc: U+0000 to U+001F and U+007F to U+009F.
        if (c.category() == QChar::Other_Control) {
            return false;
        }
    }
    return true;
}

bool printableAscii(const QString& text)
{
    for (const QChar c : text) {
        if (c.unicode() < 0x21 || c.unicode() > 0x7E) {
            return false;
        }
    }
    return true;
}

const QStringList kFamilies{QStringLiteral("lsb"),  QStringLiteral("usb"),  QStringLiteral("digl"),
                            QStringLiteral("digu"), QStringLiteral("am"),   QStringLiteral("fm"),
                            QStringLiteral("cw"),   QStringLiteral("fdvl"), QStringLiteral("fdvu")};
const QStringList kModes{QStringLiteral("LSB"),  QStringLiteral("USB"), QStringLiteral("DIGL"),
                         QStringLiteral("DIGU"), QStringLiteral("AM"),  QStringLiteral("SAM"),
                         QStringLiteral("FM"),   QStringLiteral("CWL"), QStringLiteral("CWU"),
                         QStringLiteral("FDVL"), QStringLiteral("FDVU")};

struct BandEdge {
    const char* name;
    qint64 from;
    qint64 to;
};
// Section 6.2, in frequency order.
constexpr BandEdge kBands[] = {
    {"2200m", 135700, 137800},         {"630m", 472000, 479000},
    {"160m", 1800000, 2000000},        {"80m", 3500000, 4000000},
    {"60m", 5250000, 5450000},         {"40m", 7000000, 7300000},
    {"30m", 10100000, 10150000},       {"20m", 14000000, 14350000},
    {"17m", 18068000, 18168000},       {"15m", 21000000, 21450000},
    {"12m", 24890000, 24990000},       {"10m", 28000000, 29700000},
    {"6m", 50000000, 54000000},        {"4m", 70000000, 71000000},
    {"2m", 144000000, 148000000},      {"1.25m", 222000000, 225000000},
    {"70cm", 420000000, 450000000},    {"33cm", 902000000, 928000000},
    {"23cm", 1240000000, 1300000000},
};

QStringList bandNames(const QStringList& extra)
{
    QStringList names;
    for (const BandEdge& band : kBands) {
        names.append(QLatin1String(band.name));
    }
    names.append(QStringLiteral("other"));
    names.append(extra);
    return names;
}

const QStringList kStates{QStringLiteral("talking"), QStringLiteral("unknown"),
                          QStringLiteral("doubled"), QStringLiteral("quiet")};
const QStringList kTurnKinds{QStringLiteral("talking"), QStringLiteral("unknown"),
                             QStringLiteral("doubled")};
const QStringList kMessageStates{QStringLiteral("shown"), QStringLiteral("removed"),
                                 QStringLiteral("deleted"), QStringLiteral("expired")};
const QStringList kLimitKeys{QStringLiteral("readingsPerSecond"), QStringLiteral("presencePerSecond"),
                             QStringLiteral("messagesPerMinute"), QStringLiteral("messagesPerHour"),
                             QStringLiteral("messageCodePoints"), QStringLiteral("messageLines"),
                             QStringLiteral("namesPerMinute"),    QStringLiteral("reportsPerHour"),
                             QStringLiteral("slices"),            QStringLiteral("reads")};

const QString kRoomIdPattern = QStringLiteral("(lsb|usb|digl|digu|am|fm|cw|fdvl|fdvu)-[1-9][0-9]{0,10}");

struct Kind {
    Fields fields;
    // The raw message, the decoded keys so far.
    Post post;
};
using Table = QHash<QString, Kind>;

struct Tables {
    Table fromStation;
    Table fromLive;
    Table toStation;
    Table toLive;
};

Tables buildTables()
{
    const Check versionKind = whole(1, 65535);
    const Check sid = pattern(QStringLiteral("[a-z2-7]{26}"), QStringLiteral("a station id"));
    const Check spki = base64Bytes(91);
    const Check sig = base64Bytes(64);
    const Check nonce = base64Bytes(32);
    const Check websiteKey = base64Bytes(32);
    const Check software = string(1, 64, noControl);
    const Check softwareVersion = string(1, 32, noControl);
    const Check callsign = [](const Node& v, const QString& path, QJsonValue& out, QString& why) {
        if (v.kind != Node::Kind::String || !v.scalarText || !isCallsign(v.text)) {
            return bad(why, path, QStringLiteral("not a callsign"));
        }
        out = QJsonValue(v.text);
        return true;
    };
    const Check grid = [](const Node& v, const QString& path, QJsonValue& out, QString& why) {
        if (v.kind != Node::Kind::String || !v.scalarText || !isGrid(v.text)) {
            return bad(why, path, QStringLiteral("not a grid"));
        }
        out = QJsonValue(v.text);
        return true;
    };
    const Check sliceId = whole(0, 255);
    const Check hz = whole(1, kMaxHz);
    const Check filterHz = whole(1, 1000000);
    const Check mode = oneOf(kModes);
    const Check family = oneOf(kFamilies);
    const Check band = oneOf(bandNames({}));
    const Check roomId = pattern(kRoomIdPattern, QStringLiteral("a room id"));
    const Check dbm = number(-160, 94);
    const Check unixMs = whole(0, kMaxSafe);
    const Check id = whole(1, kMaxSafe);
    const Check gen = whole(1, kMaxSafe);
    const Check count = whole(0, kMaxSafe);
    const Check nth = whole(1, 65535);
    const Check km = orNull(whole(0, 20038));
    const Check offsetHz = whole(-1000000, 1000000);
    const Check rowRef = pattern(QStringLiteral("[a-z2-7]{8}"), QStringLiteral("a row ref"));
    const Check text = string(1, 4000);
    const Check clientId = string(1, 64, printableAscii);
    const Check reason = string(1, 1024);
    const Check code = pattern(QStringLiteral("[a-z-]{1,32}"), QStringLiteral("a code"));
    const Check about = orNull(pattern(QStringLiteral("[A-Za-z]{1,32}"), QStringLiteral("a message type")));
    const Check retry = whole(0, 2147483647LL);
    const Check weblinkUrl = [](const Node& v, const QString& path, QJsonValue& out, QString& why) {
        if (!string(1, 512, printableAscii)(v, path, out, why)) {
            return false;
        }
        if (!v.text.startsWith(QLatin1String("https://"))) {
            return bad(why, path, QStringLiteral("not an https:// address"));
        }
        return true;
    };
    const Check state = oneOf(kStates);
    const Check turnKind = oneOf(kTurnKinds);
    const Check messageState = oneOf(kMessageStates);
    const Check confirm = string(1, 64);

    // ---- objects

    const Check slice = object({{QStringLiteral("id"), sliceId},
                                {QStringLiteral("hz"), hz},
                                {QStringLiteral("mode"), mode},
                                {QStringLiteral("filterHz"), filterHz},
                                {QStringLiteral("keyed"), boolean}});

    // `text` is a text exactly when `state` is shown, otherwise null; a
    // summary's last message holds at most 80 code points (6.11).
    auto stateText = [text](bool summary) {
        return [text, summary](const Node& raw, QJsonObject& out, const QString& path, QString& why) {
            const Node* value = raw.member(QStringLiteral("text"));
            if (value == nullptr) {
                return bad(why, path + QStringLiteral("text"), QStringLiteral("missing"));
            }
            if (out.value(QStringLiteral("state")).toString() == QLatin1String("shown")) {
                QJsonValue decoded;
                if (!text(*value, path + QStringLiteral("text"), decoded, why)) {
                    return false;
                }
                if (summary && value->text.toUcs4().size() > kSummaryTextCodePoints) {
                    return bad(why, path + QStringLiteral("text"),
                               QStringLiteral("longer than 80 code points"));
                }
                out.insert(QStringLiteral("text"), decoded);
            } else if (value->kind != Node::Kind::Null) {
                return bad(why, path + QStringLiteral("text"),
                           QStringLiteral("not null for a message that is not shown"));
            } else {
                out.insert(QStringLiteral("text"), QJsonValue(QJsonValue::Null));
            }
            return true;
        };
    };

    const Check lastMessage = object({{QStringLiteral("callsign"), callsign},
                                      {QStringLiteral("nth"), nth},
                                      {QStringLiteral("t"), unixMs},
                                      {QStringLiteral("state"), messageState}},
                                     stateText(true));

    const Check summary = object({{QStringLiteral("id"), roomId},
                                  {QStringLiteral("band"), band},
                                  {QStringLiteral("family"), family},
                                  {QStringLiteral("hz"), hz},
                                  {QStringLiteral("state"), state},
                                  {QStringLiteral("talkers"), array(callsign)},
                                  {QStringLiteral("members"), count},
                                  {QStringLiteral("messages"), count},
                                  {QStringLiteral("last"), unixMs},
                                  {QStringLiteral("folded"), boolean},
                                  {QStringLiteral("start"), orNull(unixMs)},
                                  {QStringLiteral("heard"), count},
                                  {QStringLiteral("readings"), array(dbm, 0, 12)},
                                  {QStringLiteral("lastMessage"), orNull(lastMessage)},
                                  {QStringLiteral("ended"), orNull(unixMs)}});

    const Check readingObject = object({{QStringLiteral("dbm"), dbm},
                                        {QStringLiteral("t"), unixMs},
                                        {QStringLiteral("present"), boolean},
                                        {QStringLiteral("turn"), orNull(id)}});

    const Check member = object({{QStringLiteral("ref"), rowRef},
                                 {QStringLiteral("callsign"), callsign},
                                 {QStringLiteral("nth"), nth},
                                 {QStringLiteral("grid"), orNull(grid)},
                                 {QStringLiteral("km"), km},
                                 {QStringLiteral("offsetHz"), offsetHz},
                                 {QStringLiteral("joining"), boolean},
                                 {QStringLiteral("away"), boolean},
                                 {QStringLiteral("keyed"), boolean},
                                 {QStringLiteral("software"), software},
                                 {QStringLiteral("softwareVersion"), softwareVersion},
                                 {QStringLiteral("reading"), orNull(readingObject)},
                                 {QStringLiteral("you"), boolean}});

    const Check talker = object({{QStringLiteral("ref"), orNull(rowRef)},
                                 {QStringLiteral("callsign"), callsign},
                                 {QStringLiteral("nth"), nth}});
    const Check nameCount = object({{QStringLiteral("name"), callsign},
                                    {QStringLiteral("count"), whole(1, kMaxSafe)}});

    const Check turn = object({{QStringLiteral("id"), id},
                               {QStringLiteral("kind"), turnKind},
                               {QStringLiteral("talkers"), array(talker)},
                               {QStringLiteral("start"), unixMs},
                               {QStringLiteral("end"), orNull(unixMs)},
                               {QStringLiteral("ending"), boolean},
                               {QStringLiteral("names"), array(nameCount)},
                               {QStringLiteral("heard"), count},
                               {QStringLiteral("mine"), orNull(callsign)}});

    const Post messageText = [shown = stateText(false), dbm](const Node& raw, QJsonObject& out,
                                                             const QString& path, QString& why) {
        if (!shown(raw, out, path, why)) {
            return false;
        }
        const Node* value = raw.member(QStringLiteral("dbm"));
        if (value == nullptr) {
            return bad(why, path + QStringLiteral("dbm"), QStringLiteral("missing"));
        }
        QJsonValue decoded;
        if (!orNull(dbm)(*value, path + QStringLiteral("dbm"), decoded, why)) {
            return false;
        }
        out.insert(QStringLiteral("dbm"), decoded);
        return true;
    };

    const Check message = object({{QStringLiteral("id"), id},
                                  {QStringLiteral("ref"), orNull(rowRef)},
                                  {QStringLiteral("callsign"), callsign},
                                  {QStringLiteral("nth"), nth},
                                  {QStringLiteral("t"), unixMs},
                                  {QStringLiteral("state"), messageState}},
                                 messageText);

    const Fields mentionFields{{QStringLiteral("room"), roomId}, {QStringLiteral("message"), message}};
    const Check mention = object(mentionFields);

    Fields limitFields;
    for (const QString& key : kLimitKeys) {
        limitFields.emplace_back(key, count);
    }
    const Check limits = object(limitFields);

    // ---- kinds

    // 5.3.1: either spki with sig, or key.
    const Post helloKeys = [spki, sig, websiteKey](const Node& raw, QJsonObject& out, const QString&,
                                                   QString& why) {
        const Node* spkiValue = raw.member(QStringLiteral("spki"));
        const Node* sigValue = raw.member(QStringLiteral("sig"));
        const Node* keyValue = raw.member(QStringLiteral("key"));
        QJsonValue decoded;
        if (spkiValue != nullptr && sigValue != nullptr && keyValue == nullptr) {
            if (!spki(*spkiValue, QStringLiteral("spki"), decoded, why)) {
                return false;
            }
            out.insert(QStringLiteral("spki"), decoded);
            if (!sig(*sigValue, QStringLiteral("sig"), decoded, why)) {
                return false;
            }
            out.insert(QStringLiteral("sig"), decoded);
            return true;
        }
        if (keyValue != nullptr && spkiValue == nullptr && sigValue == nullptr) {
            if (!websiteKey(*keyValue, QStringLiteral("key"), decoded, why)) {
                return false;
            }
            out.insert(QStringLiteral("key"), decoded);
            return true;
        }
        return bad(why, QStringLiteral("hello"), QStringLiteral("needs spki with sig, or key"));
    };

    const Post presenceSlices = [slice](const Node& raw, QJsonObject& out, const QString&, QString& why) {
        const Node* value = raw.member(QStringLiteral("slices"));
        if (value == nullptr) {
            return bad(why, QStringLiteral("slices"), QStringLiteral("missing"));
        }
        QJsonValue decoded;
        if (!array(slice, 0, kMaxSlices)(*value, QStringLiteral("slices"), decoded, why)) {
            return false;
        }
        QSet<qint64> ids;
        for (const QJsonValue& item : decoded.toArray()) {
            const qint64 sliceIdValue = item.toObject().value(QStringLiteral("id")).toInteger();
            if (ids.contains(sliceIdValue)) {
                return bad(why, QStringLiteral("slices"), QStringLiteral("a slice id repeated"));
            }
            ids.insert(sliceIdValue);
        }
        out.insert(QStringLiteral("slices"), decoded);
        return true;
    };

    // 5.3.7: the reason must be one of its target's, and a key a target
    // adds is present exactly when that is the target.
    const QHash<QString, QStringList> reportReasons{
        {QStringLiteral("message"),
         {QStringLiteral("spam"), QStringLiteral("abuse"), QStringLiteral("offTopic"), QStringLiteral("other")}},
        {QStringLiteral("turn"), {QStringLiteral("wrongName")}},
        {QStringLiteral("station"), {QStringLiteral("falseReadings")}},
    };
    const QHash<QString, QStringList> reportKeys{
        {QStringLiteral("message"), {QStringLiteral("msg")}},
        {QStringLiteral("turn"), {QStringLiteral("turn")}},
        {QStringLiteral("station"), {QStringLiteral("room"), QStringLiteral("ref")}},
    };
    const QHash<QString, Check> reportKeyKinds{
        {QStringLiteral("msg"), id},
        {QStringLiteral("turn"), id},
        {QStringLiteral("room"), roomId},
        {QStringLiteral("ref"), rowRef},
    };
    const Post reportPost = [reportReasons, reportKeys, reportKeyKinds](const Node& raw, QJsonObject& out,
                                                                        const QString&, QString& why) {
        const QString target = out.value(QStringLiteral("target")).toString();
        const Node* reasonValue = raw.member(QStringLiteral("reason"));
        if (reasonValue == nullptr) {
            return bad(why, QStringLiteral("reason"), QStringLiteral("missing"));
        }
        QJsonValue decoded;
        if (!orNull(oneOf(reportReasons.value(target)))(*reasonValue, QStringLiteral("reason"), decoded, why)) {
            return false;
        }
        out.insert(QStringLiteral("reason"), decoded);
        const QStringList mine = reportKeys.value(target);
        for (auto it = reportKeys.cbegin(); it != reportKeys.cend(); ++it) {
            for (const QString& key : it.value()) {
                const Node* value = raw.member(key);
                if (it.key() == target) {
                    if (value == nullptr) {
                        return bad(why, key, QStringLiteral("missing"));
                    }
                    if (!reportKeyKinds.value(key)(*value, key, decoded, why)) {
                        return false;
                    }
                    out.insert(key, decoded);
                } else if (value != nullptr && !mine.contains(key)) {
                    return bad(why, key, QStringLiteral("not a key of a %1 report").arg(target));
                }
            }
        }
        return true;
    };

    Tables t;
    t.fromStation = {
        {QStringLiteral("hello"),
         {{{QStringLiteral("v"), versionKind},
           {QStringLiteral("software"), software},
           {QStringLiteral("softwareVersion"), softwareVersion}},
          helloKeys}},
        {QStringLiteral("presence"),
         {{{QStringLiteral("callsign"), orNull(callsign)},
           {QStringLiteral("grid"), orNull(grid)},
           {QStringLiteral("atRadio"), boolean}},
          presenceSlices}},
        {QStringLiteral("reading"),
         {{{QStringLiteral("slice"), sliceId},
           {QStringLiteral("dbm"), dbm},
           {QStringLiteral("t"), unixMs},
           {QStringLiteral("present"), boolean}},
          {}}},
        {QStringLiteral("post"),
         {{{QStringLiteral("room"), roomId}, {QStringLiteral("text"), text}, {QStringLiteral("clientId"), clientId}},
          {}}},
        {QStringLiteral("delete"), {{{QStringLiteral("msg"), id}}, {}}},
        {QStringLiteral("name"), {{{QStringLiteral("turn"), id}, {QStringLiteral("name"), callsign}}, {}}},
        {QStringLiteral("report"),
         {{{QStringLiteral("target"),
            oneOf({QStringLiteral("message"), QStringLiteral("turn"), QStringLiteral("station")})}},
          reportPost}},
        {QStringLiteral("read"), {{{QStringLiteral("room"), roomId}}, {}}},
        {QStringLiteral("unread"), {{{QStringLiteral("room"), roomId}}, {}}},
        {QStringLiteral("resync"), {{{QStringLiteral("room"), orNull(roomId)}}, {}}},
        {QStringLiteral("mentionsSeen"), {{{QStringLiteral("t"), unixMs}}, {}}},
        {QStringLiteral("weblink"), {{}, {}}},
        {QStringLiteral("deleteArchive"), {{{QStringLiteral("confirm"), confirm}}, {}}},
        {QStringLiteral("hidden"), {{}, {}}},
    };
    t.fromLive = {
        {QStringLiteral("watchBand"), {{{QStringLiteral("band"), oneOf(bandNames({QStringLiteral("all")}))}}, {}}},
        {QStringLiteral("watch"), {{{QStringLiteral("room"), roomId}}, {}}},
        {QStringLiteral("unwatch"), {{{QStringLiteral("room"), roomId}}, {}}},
        {QStringLiteral("resync"), {{{QStringLiteral("room"), orNull(roomId)}}, {}}},
    };

    const Kind roomsKind{{{QStringLiteral("gen"), gen}, {QStringLiteral("rooms"), array(summary)}}, {}};
    const Kind roomsDeltaKind{{{QStringLiteral("gen"), gen},
                               {QStringLiteral("add"), array(summary)},
                               {QStringLiteral("change"), array(summary)},
                               {QStringLiteral("remove"), array(roomId)}},
                              {}};
    const Kind roomKind{{{QStringLiteral("gen"), gen},
                         {QStringLiteral("id"), roomId},
                         {QStringLiteral("opening"), id},
                         {QStringLiteral("band"), band},
                         {QStringLiteral("family"), family},
                         {QStringLiteral("hz"), hz},
                         {QStringLiteral("members"), array(member)},
                         {QStringLiteral("now"), array(turn)},
                         {QStringLiteral("earlier"), array(turn)},
                         {QStringLiteral("messages"), array(message)}},
                        {}};
    const Kind deltaKind{{{QStringLiteral("room"), roomId},
                          {QStringLiteral("gen"), gen},
                          {QStringLiteral("members"), array(member)},
                          {QStringLiteral("left"), array(rowRef)},
                          {QStringLiteral("turns"), array(turn)},
                          {QStringLiteral("messages"), array(message)},
                          {QStringLiteral("closed"), boolean}},
                         {}};
    const Kind errorKind{{{QStringLiteral("code"), code},
                          {QStringLiteral("reason"), reason},
                          {QStringLiteral("about"), about},
                          {QStringLiteral("retryAfterMs"), retry}},
                         {}};

    t.toStation = {
        {QStringLiteral("challenge"), {{{QStringLiteral("nonce"), nonce}}, {}}},
        {QStringLiteral("welcome"),
         {{{QStringLiteral("station"), sid},
           {QStringLiteral("kind"), oneOf({QStringLiteral("core"), QStringLiteral("key")})},
           {QStringLiteral("callsign"), orNull(callsign)},
           {QStringLiteral("grid"), orNull(grid)},
           {QStringLiteral("time"), unixMs},
           {QStringLiteral("limits"), limits},
           {QStringLiteral("mentions"), array(mention)}},
          {}}},
        {QStringLiteral("rooms"), roomsKind},
        {QStringLiteral("roomsDelta"), roomsDeltaKind},
        {QStringLiteral("room"), roomKind},
        {QStringLiteral("delta"), deltaKind},
        {QStringLiteral("posted"),
         {{{QStringLiteral("clientId"), clientId},
           {QStringLiteral("ok"), boolean},
           {QStringLiteral("msg"), orNull(id)},
           {QStringLiteral("code"), orNull(code)},
           {QStringLiteral("reason"), orNull(reason)}},
          {}}},
        {QStringLiteral("mention"), {mentionFields, {}}},
        {QStringLiteral("notice"),
         {{{QStringLiteral("code"), code}, {QStringLiteral("text"), reason}, {QStringLiteral("until"), orNull(unixMs)}},
          {}}},
        {QStringLiteral("weblinkOk"),
         {{{QStringLiteral("url"), weblinkUrl}, {QStringLiteral("expires"), unixMs}}, {}}},
        {QStringLiteral("error"), errorKind},
    };
    t.toLive = {
        {QStringLiteral("rooms"), roomsKind},   {QStringLiteral("roomsDelta"), roomsDeltaKind},
        {QStringLiteral("room"), roomKind},     {QStringLiteral("delta"), deltaKind},
        {QStringLiteral("error"), errorKind},
    };
    return t;
}

const Tables& tables()
{
    static const Tables built = buildTables();
    return built;
}

const Table& tableFor(Direction direction)
{
    switch (direction) {
    case Direction::StationToServer:
        return tables().fromStation;
    case Direction::LiveToServer:
        return tables().fromLive;
    case Direction::ServerToStation:
        return tables().toStation;
    case Direction::ServerToLive:
        break;
    }
    return tables().toLive;
}

// ----------------------------------------------------------------- encode

void writeString(const QString& text, QByteArray& out)
{
    out.append('"');
    const QByteArray utf8 = text.toUtf8();
    for (const char ch : utf8) {
        const auto c = static_cast<unsigned char>(ch);
        switch (c) {
        case '"': out.append("\\\""); break;
        case '\\': out.append("\\\\"); break;
        case '\n': out.append("\\n"); break;
        case '\r': out.append("\\r"); break;
        case '\t': out.append("\\t"); break;
        case 0x08: out.append("\\b"); break;
        case 0x0C: out.append("\\f"); break;
        default:
            if (c < 0x20) {
                out.append(QStringLiteral("\\u%1").arg(static_cast<uint>(c), 4, 16, QLatin1Char('0')).toLatin1());
            } else {
                out.append(ch);
            }
            break;
        }
    }
    out.append('"');
}

void writeValue(const QJsonValue& value, QByteArray& out);

void writeObject(const QJsonObject& object, QByteArray& out)
{
    out.append('{');
    bool first = true;
    auto writeMember = [&](const QString& key, const QJsonValue& value) {
        if (!first) {
            out.append(',');
        }
        first = false;
        writeString(key, out);
        out.append(':');
        writeValue(value, out);
    };
    // `type` first, so a log line reads as the spec's examples do.
    const auto typeIt = object.constFind(QStringLiteral("type"));
    if (typeIt != object.constEnd()) {
        writeMember(typeIt.key(), typeIt.value());
    }
    for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
        if (it.key() != QLatin1String("type")) {
            writeMember(it.key(), it.value());
        }
    }
    out.append('}');
}

void writeValue(const QJsonValue& value, QByteArray& out)
{
    switch (value.type()) {
    case QJsonValue::Null:
    case QJsonValue::Undefined:
        out.append("null");
        break;
    case QJsonValue::Bool:
        out.append(value.toBool() ? "true" : "false");
        break;
    case QJsonValue::Double: {
        const double d = value.toDouble();
        if (!std::isfinite(d)) {
            out.append("null");
        } else if (std::floor(d) == d && std::fabs(d) <= static_cast<double>(kMaxSafe)) {
            out.append(QByteArray::number(value.toInteger(static_cast<qint64>(d))));
        } else {
            // Shortest round trip, fixed notation: never an exponent (5.1).
            out.append(QByteArray::number(d, 'f', QLocale::FloatingPointShortest));
        }
        break;
    }
    case QJsonValue::String:
        writeString(value.toString(), out);
        break;
    case QJsonValue::Array: {
        out.append('[');
        bool first = true;
        for (const QJsonValue& item : value.toArray()) {
            if (!first) {
                out.append(',');
            }
            first = false;
            writeValue(item, out);
        }
        out.append(']');
        break;
    }
    case QJsonValue::Object:
        writeObject(value.toObject(), out);
        break;
    }
}

QJsonValue orNullText(const QString& text)
{
    return text.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(text);
}

QJsonObject kindOnly(const char* type)
{
    return QJsonObject{{QStringLiteral("type"), QLatin1String(type)}};
}

QString base32Lower(const QByteArray& bytes)
{
    static constexpr char kAlphabet[] = "abcdefghijklmnopqrstuvwxyz234567";
    QString out;
    quint32 buffer = 0;
    int bits = 0;
    for (const char ch : bytes) {
        buffer = (buffer << 8) | static_cast<unsigned char>(ch);
        bits += 8;
        while (bits >= 5) {
            out.append(QLatin1Char(kAlphabet[(buffer >> (bits - 5)) & 0x1F]));
            bits -= 5;
        }
    }
    if (bits > 0) {
        out.append(QLatin1Char(kAlphabet[(buffer << (5 - bits)) & 0x1F]));
    }
    return out;
}

} // namespace

// ----------------------------------------------------------------- public

Decoded decode(const QString& text, Direction direction)
{
    Decoded result;
    Node root;
    Parser parser(text);
    if (!parser.parseDocument(root, result.error)) {
        return result;
    }
    if (root.kind != Node::Kind::Object) {
        result.error = QStringLiteral("not an object");
        return result;
    }
    const Node* type = root.member(QStringLiteral("type"));
    if (type == nullptr || type->kind != Node::Kind::String) {
        result.error = QStringLiteral("no string type");
        return result;
    }
    result.type = type->text;
    const Table& table = tableFor(direction);
    const auto kind = table.constFind(type->text);
    if (kind == table.constEnd()) {
        result.error = QStringLiteral("unknown kind %1").arg(type->text);
        return result;
    }
    QJsonObject out{{QStringLiteral("type"), type->text}};
    if (!takeFields(root, kind->fields, QString(), out, result.error)) {
        return result;
    }
    if (kind->post && !kind->post(root, out, QString(), result.error)) {
        return result;
    }
    result.ok = true;
    result.message = out;
    return result;
}

QByteArray encode(const QJsonObject& message)
{
    QByteArray out;
    writeObject(message, out);
    return out;
}

QString memberSourceText(const QString& text, const QString& key)
{
    Node root;
    QString why;
    Parser parser(text);
    if (!parser.parseDocument(root, why) || root.kind != Node::Kind::Object) {
        return {};
    }
    const Node* value = root.member(key);
    if (value == nullptr) {
        return {};
    }
    return text.mid(value->start, value->end - value->start);
}

QJsonObject hello(const QString& software, const QString& softwareVersion, const QByteArray& spki,
                  const QByteArray& signature)
{
    return QJsonObject{{QStringLiteral("type"), QStringLiteral("hello")},
                       {QStringLiteral("v"), kVersion},
                       {QStringLiteral("software"), software},
                       {QStringLiteral("softwareVersion"), softwareVersion},
                       {QStringLiteral("spki"), toBase64Url(spki)},
                       {QStringLiteral("sig"), toBase64Url(signature)}};
}

QJsonObject helloWithKey(const QString& software, const QString& softwareVersion,
                         const QByteArray& websiteKey)
{
    return QJsonObject{{QStringLiteral("type"), QStringLiteral("hello")},
                       {QStringLiteral("v"), kVersion},
                       {QStringLiteral("software"), software},
                       {QStringLiteral("softwareVersion"), softwareVersion},
                       {QStringLiteral("key"), toBase64Url(websiteKey)}};
}

QJsonObject presence(const QString& callsign, const QString& grid, bool atRadio,
                     const QList<RoundtableSliceState>& slices)
{
    QJsonArray list;
    for (const RoundtableSliceState& slice : slices) {
        list.append(QJsonObject{{QStringLiteral("id"), slice.id},
                                {QStringLiteral("hz"), slice.hz},
                                {QStringLiteral("mode"), slice.mode},
                                {QStringLiteral("filterHz"), slice.filterHz},
                                {QStringLiteral("keyed"), slice.keyed}});
    }
    return QJsonObject{{QStringLiteral("type"), QStringLiteral("presence")},
                       {QStringLiteral("callsign"), orNullText(callsign)},
                       {QStringLiteral("grid"), orNullText(grid)},
                       {QStringLiteral("atRadio"), atRadio},
                       {QStringLiteral("slices"), list}};
}

QJsonObject reading(int sliceId, double dbm, qint64 unixMs, bool present)
{
    return QJsonObject{{QStringLiteral("type"), QStringLiteral("reading")},
                       {QStringLiteral("slice"), sliceId},
                       {QStringLiteral("dbm"), dbm},
                       {QStringLiteral("t"), unixMs},
                       {QStringLiteral("present"), present}};
}

QJsonObject post(const QString& roomId, const QString& text, const QString& clientId)
{
    return QJsonObject{{QStringLiteral("type"), QStringLiteral("post")},
                       {QStringLiteral("room"), roomId},
                       {QStringLiteral("text"), text},
                       {QStringLiteral("clientId"), clientId}};
}

QJsonObject deleteMessage(qint64 messageId)
{
    return QJsonObject{{QStringLiteral("type"), QStringLiteral("delete")}, {QStringLiteral("msg"), messageId}};
}

QJsonObject name(qint64 turnId, const QString& callsign)
{
    return QJsonObject{{QStringLiteral("type"), QStringLiteral("name")},
                       {QStringLiteral("turn"), turnId},
                       {QStringLiteral("name"), callsign}};
}

QJsonObject reportMessage(qint64 messageId, const QString& reason)
{
    return QJsonObject{{QStringLiteral("type"), QStringLiteral("report")},
                       {QStringLiteral("target"), QStringLiteral("message")},
                       {QStringLiteral("msg"), messageId},
                       {QStringLiteral("reason"), orNullText(reason)}};
}

QJsonObject reportTurn(qint64 turnId, const QString& reason)
{
    return QJsonObject{{QStringLiteral("type"), QStringLiteral("report")},
                       {QStringLiteral("target"), QStringLiteral("turn")},
                       {QStringLiteral("turn"), turnId},
                       {QStringLiteral("reason"), orNullText(reason)}};
}

QJsonObject reportStation(const QString& roomId, const QString& ref, const QString& reason)
{
    return QJsonObject{{QStringLiteral("type"), QStringLiteral("report")},
                       {QStringLiteral("target"), QStringLiteral("station")},
                       {QStringLiteral("room"), roomId},
                       {QStringLiteral("ref"), ref},
                       {QStringLiteral("reason"), orNullText(reason)}};
}

QJsonObject read(const QString& roomId)
{
    return QJsonObject{{QStringLiteral("type"), QStringLiteral("read")}, {QStringLiteral("room"), roomId}};
}

QJsonObject unread(const QString& roomId)
{
    return QJsonObject{{QStringLiteral("type"), QStringLiteral("unread")}, {QStringLiteral("room"), roomId}};
}

QJsonObject resync(const QString& roomId)
{
    return QJsonObject{{QStringLiteral("type"), QStringLiteral("resync")},
                       {QStringLiteral("room"), orNullText(roomId)}};
}

QJsonObject mentionsSeen(qint64 unixMs)
{
    return QJsonObject{{QStringLiteral("type"), QStringLiteral("mentionsSeen")}, {QStringLiteral("t"), unixMs}};
}

QJsonObject weblink()
{
    return kindOnly("weblink");
}

QJsonObject deleteArchive(const QString& confirm)
{
    return QJsonObject{{QStringLiteral("type"), QStringLiteral("deleteArchive")},
                       {QStringLiteral("confirm"), confirm}};
}

QJsonObject hidden()
{
    return kindOnly("hidden");
}

QJsonObject watchBand(const QString& band)
{
    return QJsonObject{{QStringLiteral("type"), QStringLiteral("watchBand")}, {QStringLiteral("band"), band}};
}

QJsonObject watch(const QString& roomId)
{
    return QJsonObject{{QStringLiteral("type"), QStringLiteral("watch")}, {QStringLiteral("room"), roomId}};
}

QJsonObject unwatch(const QString& roomId)
{
    return QJsonObject{{QStringLiteral("type"), QStringLiteral("unwatch")}, {QStringLiteral("room"), roomId}};
}

QByteArray helloTranscript(const QString& host, const QByteArray& nonce)
{
    QByteArray transcript("NereusSDR roundtable hello v1\n");
    transcript.append(host.toLatin1());
    transcript.append('\n');
    transcript.append(nonce);
    return transcript;
}

QString signingHost(const QUrl& url)
{
    QString host = url.host(QUrl::FullyEncoded).toLower();
    while (host.endsWith(QLatin1Char('.'))) {
        host.chop(1);
    }
    return host;
}

QString roundtableId(const QByteArray& spki)
{
    QByteArray input("NereusSDR roundtable id v1\n");
    input.append(spki);
    return base32Lower(QCryptographicHash::hash(input, QCryptographicHash::Sha256)).left(26);
}

QByteArray fromBase64Url(const QString& text, bool* ok)
{
    auto refuse = [ok]() {
        if (ok != nullptr) {
            *ok = false;
        }
        return QByteArray();
    };
    for (const QChar c : text) {
        const ushort u = c.unicode();
        const bool allowed = (u >= 'A' && u <= 'Z') || (u >= 'a' && u <= 'z') || (u >= '0' && u <= '9')
            || u == '-' || u == '_';
        if (!allowed) {
            return refuse();
        }
    }
    if (text.size() % 4 == 1) {
        return refuse();
    }
    const QByteArray bytes = QByteArray::fromBase64(
        text.toLatin1(), QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
    // The unused low bits of the last character must be zero: the text is
    // exactly what the bytes encode to.
    if (toBase64Url(bytes) != text) {
        return refuse();
    }
    if (ok != nullptr) {
        *ok = true;
    }
    return bytes;
}

QString toBase64Url(const QByteArray& bytes)
{
    return QString::fromLatin1(
        bytes.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}

bool isCallsign(const QString& text)
{
    if (text.size() < 3 || text.size() > 16) {
        return false;
    }
    bool letter = false;
    bool digit = false;
    for (const QChar c : text) {
        const ushort u = c.unicode();
        if ((u >= 'A' && u <= 'Z') || (u >= 'a' && u <= 'z')) {
            letter = true;
        } else if (u >= '0' && u <= '9') {
            digit = true;
        } else if (u != '/') {
            return false;
        }
    }
    if (text.startsWith(QLatin1Char('/')) || text.endsWith(QLatin1Char('/'))
        || text.contains(QLatin1String("//"))) {
        return false;
    }
    return letter && digit;
}

QString baseCallsign(const QString& callsign)
{
    QString longest;
    for (const QString& part : callsign.split(QLatin1Char('/'))) {
        if (part.size() > longest.size()) {
            longest = part;
        }
    }
    return longest.toUpper();
}

bool isGrid(const QString& text)
{
    static const QRegularExpression re(QStringLiteral("\\A[A-Ra-r]{2}[0-9]{2}(?:[A-Xa-x]{2})?\\z"));
    return re.match(text).hasMatch();
}

QString gridToSend(const QString& text)
{
    QString grid = text.trimmed();
    if (grid.size() == 8) {
        grid.truncate(6);
    }
    if (!isGrid(grid)) {
        return {};
    }
    return grid.left(2).toUpper() + grid.mid(2, 2) + grid.mid(4).toLower();
}

QString familyOfMode(const QString& mode)
{
    static const QHash<QString, QString> families{
        {QStringLiteral("LSB"), QStringLiteral("lsb")},   {QStringLiteral("USB"), QStringLiteral("usb")},
        {QStringLiteral("DIGL"), QStringLiteral("digl")}, {QStringLiteral("DIGU"), QStringLiteral("digu")},
        {QStringLiteral("AM"), QStringLiteral("am")},     {QStringLiteral("SAM"), QStringLiteral("am")},
        {QStringLiteral("FM"), QStringLiteral("fm")},     {QStringLiteral("CWL"), QStringLiteral("cw")},
        {QStringLiteral("CWU"), QStringLiteral("cw")},    {QStringLiteral("FDVL"), QStringLiteral("fdvl")},
        {QStringLiteral("FDVU"), QStringLiteral("fdvu")},
    };
    return families.value(mode);
}

QString bandOf(qint64 hz)
{
    for (const BandEdge& band : kBands) {
        if (hz >= band.from && hz <= band.to) {
            return QLatin1String(band.name);
        }
    }
    return QStringLiteral("other");
}

QStringList mentionedBaseCallsigns(const QString& text)
{
    static const QRegularExpression word(QStringLiteral("@?([A-Za-z0-9/]+)"));
    QStringList bases;
    auto it = word.globalMatch(text);
    while (it.hasNext()) {
        const QString base = baseCallsign(it.next().captured(1));
        if (!base.isEmpty() && !bases.contains(base)) {
            bases.append(base);
        }
    }
    return bases;
}

} // namespace NereusSDR::RoundtableWire
