// =================================================================
// src/core/session/IceDiagnostics.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. See IceDiagnostics.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-29 - Created for the 5G media-path diagnosis. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30 - Review fix: the ICE username fragment, a failed ufrag
//                 check's fragments and the STUN username, realm and nonce
//                 are hidden (isSecretLine). J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/session/IceDiagnostics.h"

#include "core/session/CoreAddresses.h"

#include <QElapsedTimer>
#include <QHostAddress>
#include <QLoggingCategory>
#include <QMutexLocker>
#include <QRegularExpression>

#include <rtc/rtc.hpp>

#include <atomic>
#include <memory>
#include <utility>

Q_LOGGING_CATEGORY(lcIceDiag, "nereus.session.icediag")

namespace NereusSDR {

namespace {

bool isRunChar(QChar c)
{
    const char16_t u = c.unicode();
    return (u >= '0' && u <= '9') || (u >= 'a' && u <= 'f') || (u >= 'A' && u <= 'F')
        || u == ':' || u == '.';
}

bool isScopeChar(QChar c)
{
    const char16_t u = c.unicode();
    return (u >= '0' && u <= '9') || (u >= 'a' && u <= 'z') || (u >= 'A' && u <= 'Z')
        || u == '_' || u == '-';
}

// A letter, digit or underscore next to a match: the match is part of a
// word or an identifier ("rtc::impl"), not an address on its own.
bool isWordChar(QChar c)
{
    return c.isLetterOrNumber() || c == QLatin1Char('_');
}

bool strictIpv4(const QString& text)
{
    const QStringList parts = text.split(QLatin1Char('.'));
    if (parts.size() != 4) {
        return false;
    }
    for (const QString& part : parts) {
        if (part.isEmpty() || part.size() > 3) {
            return false;
        }
        for (const QChar c : part) {
            if (!c.isDigit() || c.unicode() > 127) {
                return false;
            }
        }
        if (part.toInt() > 255) {
            return false;
        }
    }
    return true;
}

bool hasDecimalDigit(const QString& text)
{
    for (const QChar c : text) {
        if (c.unicode() >= '0' && c.unicode() <= '9') {
            return true;
        }
    }
    return false;
}

// Canonical form; an IPv4-mapped IPv6 address is its IPv4 address.
QHostAddress canonical(const QString& text)
{
    QHostAddress address(text);
    if (address.protocol() == QAbstractSocket::IPv6Protocol) {
        bool mapped = false;
        const quint32 v4 = address.toIPv4Address(&mapped);
        if (mapped) {
            address = QHostAddress(v4);
        }
    }
    address.setScopeId(QString());
    return address;
}

QString keyFor(const QHostAddress& address)
{
    return (address.protocol() == QAbstractSocket::IPv4Protocol ? QStringLiteral("4/")
                                                                 : QStringLiteral("6/"))
        + address.toString();
}

} // namespace

// ---------------------------------------------------------------------------
// IceAddressRedactor

void IceAddressRedactor::setLocalAddresses(const QList<QNetworkAddressEntry>& entries)
{
    QMutexLocker lock(&m_mutex);
    loadLocalLocked(entries);
}

void IceAddressRedactor::setLocalSource(LocalSource source)
{
    QMutexLocker lock(&m_mutex);
    m_localSource = std::move(source);
}

void IceAddressRedactor::loadLocalLocked(const QList<QNetworkAddressEntry>& entries)
{
    m_local.clear();
    m_localOrder.clear();
    for (const QNetworkAddressEntry& entry : entries) {
        const QHostAddress ip = canonical(entry.ip().toString());
        if (ip.isNull() || ip.isLoopback()) {
            continue;
        }
        const QString key = keyFor(ip);
        Local local;
        if (ip.protocol() == QAbstractSocket::IPv6Protocol) {
            local.temporary = entry.dnsEligibility() == QNetworkAddressEntry::DnsIneligible;
            local.deprecated = entry.isLifetimeKnown() && entry.preferredLifetime().hasExpired();
            const Q_IPV6ADDR bytes = ip.toIPv6Address();
            local.eui64 = bytes[11] == 0xff && bytes[12] == 0xfe;
        }
        if (!m_local.contains(key)) {
            m_localOrder.append(key);
        }
        m_local.insert(key, local);
    }
}

bool IceAddressRedactor::findInRunLocked(const QString& run, QChar before, QChar after,
                                         int from, Match* match) const
{
    const int size = static_cast<int>(run.size());
    for (int start = from; start < size; ++start) {
        if (!(start == 0 || (start == from && from == 0) || run.at(start - 1) == QLatin1Char(':'))) {
            continue;
        }
        const QChar left = start == 0 ? before : run.at(start - 1);
        // Ends: the end of the run and every ':' or '.' after the start,
        // longest first.
        for (int end = size; end > start; --end) {
            if (end != size && run.at(end) != QLatin1Char(':') && run.at(end) != QLatin1Char('.')) {
                continue;
            }
            const QString text = run.mid(start, end - start);
            const QChar right = end == size ? after : run.at(end);
            const bool glued = isWordChar(left) || isWordChar(right);
            const int colons = static_cast<int>(text.count(QLatin1Char(':')));

            Match found;
            found.start = start;
            found.end = end;
            if (colons == 0) {
                if (!strictIpv4(text)) {
                    continue;
                }
            } else {
                if (colons < 2 || (glued && colons < 3)) {
                    continue;
                }
                if (text == QLatin1String("::")) {
                    found.fixed = QStringLiteral("unspecified");
                    found.ipv6 = true;
                    *match = found;
                    return true;
                }
                if (colons < 3 && !hasDecimalDigit(text)) {
                    continue; // "face::add"
                }
                if (QHostAddress(text).protocol() != QAbstractSocket::IPv6Protocol) {
                    continue;
                }
                found.ipv6 = true;
            }

            QHostAddress address = canonical(text);
            // libjuice writes "host:port" for IPv6 too, unbracketed; a port
            // of up to four digits then reads as the last group. Prefer the
            // shorter address when it is one already seen, or loopback.
            if (found.ipv6 && colons >= 3) {
                const int lastColon = static_cast<int>(text.lastIndexOf(QLatin1Char(':')));
                const QString tail = text.mid(lastColon + 1);
                bool decimal = !tail.isEmpty() && tail.size() <= 5;
                for (const QChar c : tail) {
                    decimal = decimal && c.unicode() >= '0' && c.unicode() <= '9';
                }
                const QString head = text.left(lastColon);
                const QHostAddress headAddress = canonical(head);
                if (decimal && head.count(QLatin1Char(':')) >= 2
                    && QHostAddress(head).protocol() == QAbstractSocket::IPv6Protocol
                    && (headAddress.isLoopback() || m_known.contains(keyFor(headAddress)))) {
                    found.end = start + lastColon;
                    address = headAddress;
                }
            }
            if (address.isLoopback()) {
                found.fixed = QStringLiteral("loopback");
            } else if (address == QHostAddress::AnyIPv4 || address == QHostAddress::AnyIPv6) {
                found.fixed = QStringLiteral("unspecified");
            } else {
                found.key = keyFor(address);
            }
            found.ipv6 = address.protocol() == QAbstractSocket::IPv6Protocol;
            *match = found;
            return true;
        }
    }
    return false;
}

QString IceAddressRedactor::keyOfLocked(const QString& text) const
{
    QString value = text;
    if (value.startsWith(QLatin1Char('['))) {
        value = value.mid(1);
    }
    int stop = 0;
    while (stop < value.size() && isRunChar(value.at(stop))) {
        ++stop;
    }
    Match match;
    if (!findInRunLocked(value.left(stop), QLatin1Char(' '), QLatin1Char(' '), 0, &match)
        || match.start != 0) {
        return {};
    }
    return match.key;
}

void IceAddressRedactor::learnLocked(const QString& addressText, const QString& type)
{
    const QString key = keyOfLocked(addressText);
    if (key.isEmpty()) {
        return;
    }
    Known& known = m_known[key];
    if (known.type.isEmpty()) {
        known.type = type;
    }
}

void IceAddressRedactor::learnTypesLocked(const QString& text)
{
    // "<address> <port> typ <type> [raddr <address> rport <port>]", as in
    // candidate lines and SDP; libjuice's mapped and relayed addresses.
    static const QRegularExpression candidate(QStringLiteral(
        "(\\S+)\\s+\\d+\\s+typ\\s+(host|srflx|prflx|relay)\\b(?:[^\\n]*?\\braddr\\s+(\\S+))?"));
    static const QRegularExpression mapped(QStringLiteral("mapped address (\\S+)"));
    static const QRegularExpression relayed(QStringLiteral("relayed address (\\S+)"));

    auto it = candidate.globalMatch(text);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        const QString type = m.captured(2);
        learnLocked(m.captured(1), type);
        if (m.hasCaptured(3) && !m.captured(3).isEmpty()) {
            const QString related = type == QLatin1String("relay") ? QStringLiteral("srflx")
                                                                   : QStringLiteral("host");
            learnLocked(m.captured(3), related);
        }
    }
    auto maps = mapped.globalMatch(text);
    while (maps.hasNext()) {
        learnLocked(maps.next().captured(1), QStringLiteral("srflx"));
    }
    auto relays = relayed.globalMatch(text);
    while (relays.hasNext()) {
        learnLocked(relays.next().captured(1), QStringLiteral("relay"));
    }
}

QString IceAddressRedactor::tokenLocked(const Match& match)
{
    if (!match.fixed.isEmpty()) {
        return match.fixed;
    }
    Known& known = m_known[match.key];
    if (known.name.isEmpty()) {
        known.name = match.ipv6 ? QStringLiteral("v6#%1").arg(m_nextV6++)
                                : QStringLiteral("v4#%1").arg(m_nextV4++);
        // A new IPv6 address that is not one of ours may be a privacy
        // address made since the table was read.
        if (match.ipv6 && !m_local.contains(match.key) && m_localSource) {
            loadLocalLocked(m_localSource());
        }
    }
    QString token = QLatin1Char('<') + known.name;
    if (!known.type.isEmpty()) {
        token += QLatin1Char(' ') + known.type;
    }
    const auto local = m_local.constFind(match.key);
    if (match.ipv6 && local != m_local.cend()) {
        token += local->temporary ? QStringLiteral(" temporary") : QStringLiteral(" stable");
        if (local->deprecated) {
            token += QStringLiteral(" deprecated");
        }
        if (local->eui64) {
            token += QStringLiteral(" eui64");
        }
    }
    return token + QLatin1Char('>');
}

QString IceAddressRedactor::redact(const QString& text)
{
    QMutexLocker lock(&m_mutex);
    learnTypesLocked(text);

    QString out;
    out.reserve(text.size() + 16);
    const int size = static_cast<int>(text.size());
    int i = 0;
    while (i < size) {
        if (!isRunChar(text.at(i))) {
            out += text.at(i);
            ++i;
            continue;
        }
        int j = i;
        while (j < size && isRunChar(text.at(j))) {
            ++j;
        }
        const QString run = text.mid(i, j - i);
        const QChar before = i > 0 ? text.at(i - 1) : QLatin1Char(' ');
        const QChar after = j < size ? text.at(j) : QLatin1Char(' ');
        // Nine or more colons and no dot: a fingerprint, not an address
        // (the longest address with a port has eight).
        if (run.count(QLatin1Char(':')) >= 9 && !run.contains(QLatin1Char('.'))) {
            out += run;
            i = j;
            continue;
        }
        int pos = 0;
        int next = j;
        Match match;
        while (pos < run.size() && findInRunLocked(run, before, after, pos, &match)) {
            out += run.mid(pos, match.start - pos);
            QString scope;
            int k = j;
            const bool atEnd = match.end == run.size();
            if (atEnd && match.ipv6 && k < size && text.at(k) == QLatin1Char('%')) {
                int s = k + 1;
                while (s < size && isScopeChar(text.at(s))) {
                    ++s;
                }
                if (s > k + 1) {
                    scope = text.mid(k, s - k);
                    k = s;
                }
            }
            if (atEnd && match.start == 0 && before == QLatin1Char('[') && k < size
                && text.at(k) == QLatin1Char(']') && out.endsWith(QLatin1Char('['))) {
                out.chop(1);
                ++k;
            }
            out += tokenLocked(match) + scope;
            pos = match.end;
            if (atEnd) {
                next = k;
            }
        }
        out += run.mid(pos);
        i = next;
    }
    return out;
}

QString IceAddressRedactor::localSummary()
{
    QMutexLocker lock(&m_mutex);
    QStringList v6;
    QStringList v4;
    for (const QString& key : std::as_const(m_localOrder)) {
        Match match;
        match.key = key;
        match.ipv6 = key.startsWith(QLatin1String("6/"));
        (match.ipv6 ? v6 : v4).append(tokenLocked(match));
    }
    return (v6 + v4).join(QLatin1Char(' '));
}

// ---------------------------------------------------------------------------
// IceDiagnostics

namespace IceDiagnostics {

namespace {

std::atomic<bool> g_enabled{false};

struct State {
    QMutex mutex;
    Sink sink;
    std::shared_ptr<IceAddressRedactor> redactor;
    QElapsedTimer clock;
};

State& state()
{
    static State s;
    return s;
}

// libjuice prints the ICE password and username fragment in its
// description lines ("ufrag=", "pwd=", and "a=ice-ufrag:", "a=ice-pwd:" in
// the generated description's first line), the fragments of a failed
// ufrag check ("expected=", "actual="), one warning's password, and a STUN
// message's username, realm and nonce. They go with the addresses.
QString hideSecrets(QString line)
{
    static const QRegularExpression quoted(QStringLiteral(
        "((?:password|pwd|ufrag|expected|actual)=\")[^\"]*"));
    static const QRegularExpression bare(QStringLiteral("(ice-pwd:|ice-ufrag:)\\S*"));
    // A realm may hold spaces: the rest of the line goes.
    static const QRegularExpression stun(QStringLiteral("(Got (?:username|realm|nonce): ).*$"));
    line.replace(quoted, QStringLiteral("\\1<hidden>"));
    line.replace(bare, QStringLiteral("\\1<hidden>"));
    return line.replace(stun, QStringLiteral("\\1<hidden>"));
}

void write(const char* tag, const QString& text)
{
    Sink sink;
    std::shared_ptr<IceAddressRedactor> redactor;
    qint64 ms = 0;
    {
        State& s = state();
        QMutexLocker lock(&s.mutex);
        if (!s.sink || !s.redactor) {
            return;
        }
        sink = s.sink;
        redactor = s.redactor;
        ms = s.clock.elapsed();
    }
    const QStringList lines = text.split(QLatin1Char('\n'));
    for (QString line : lines) {
        if (line.endsWith(QLatin1Char('\r'))) {
            line.chop(1);
        }
        if (line.trimmed().isEmpty()) {
            continue;
        }
        // Redacted before anything else sees it.
        sink(QStringLiteral("+%1ms [%2] %3")
                 .arg(ms)
                 .arg(QLatin1String(tag), redactor->redact(hideSecrets(line))));
    }
}

// Session description lines other than the candidates and the connection
// line: the codecs and the ICE password, nothing about the checks.
bool isQuietSdpLine(const QString& line)
{
    if (line.size() < 2 || line.at(1) != QLatin1Char('=') || !line.at(0).isLower()) {
        return false;
    }
    return !line.startsWith(QLatin1String("a=candidate:"))
        && !line.startsWith(QLatin1String("a=end-of-candidates"))
        && !line.startsWith(QLatin1String("c="));
}

void libraryLine(rtc::LogLevel, const std::string& message)
{
    if (!g_enabled.load(std::memory_order_relaxed)) {
        return;
    }
    const QStringList lines = QString::fromStdString(message).split(QLatin1Char('\n'));
    for (QString line : lines) {
        if (line.endsWith(QLatin1Char('\r'))) {
            line.chop(1);
        }
        if (isQuietSdpLine(line) || isSecretLine(line) || isLibraryNoise(line)) {
            continue;
        }
        if (line.startsWith(QLatin1String("rtc::impl::"))) {
            line = line.mid(11);
        }
        write("lib", line);
    }
}

void install(Sink sink, IceAddressRedactor::LocalSource localSource)
{
    State& s = state();
    if (!sink) {
        g_enabled.store(false);
        // Keep a callback installed: libdatachannel writes to standard
        // output, unredacted, when its logger has no callback.
        rtc::InitLogger(rtc::LogLevel::None, [](rtc::LogLevel, const std::string&) {});
        QMutexLocker lock(&s.mutex);
        s.sink = {};
        s.redactor.reset();
        return;
    }
    auto redactor = std::make_shared<IceAddressRedactor>();
    if (localSource) {
        redactor->setLocalAddresses(localSource());
        redactor->setLocalSource(std::move(localSource));
    }
    {
        QMutexLocker lock(&s.mutex);
        s.sink = std::move(sink);
        s.redactor = redactor;
        s.clock.start();
    }
    g_enabled.store(true);
    // Verbose: libjuice's pair checks reach libdatachannel's log at its
    // DEBUG level, which libdatachannel forwards as verbose, and each ICE
    // agent takes its level from this one when it is made.
    rtc::InitLogger(rtc::LogLevel::Verbose, &libraryLine);
    write("core", QStringLiteral("ICE diagnostics on; own addresses: %1")
                      .arg(redactor->localSummary()));
}

} // namespace

bool switchRequested(const QByteArray& value)
{
    const QByteArray v = value.trimmed().toLower();
    return v == "1" || v == "true" || v == "on" || v == "yes";
}

bool enabled()
{
    return g_enabled.load(std::memory_order_relaxed);
}

void installFromEnvironment()
{
    if (!switchRequested(qgetenv(kEnvironmentVariable))) {
        return;
    }
    install([](const QString& line) { qCInfo(lcIceDiag).noquote() << line; },
            &CoreAddresses::localEntries);
}

void installForTest(Sink sink, IceAddressRedactor::LocalSource localSource)
{
    install(std::move(sink), std::move(localSource));
}

bool isSecretLine(const QString& line)
{
    // libjuice's STUN username (the two ICE username fragments), TURN
    // realm and nonce, one line per message read (stun.c). Left out whole;
    // hideSecrets() hides their values on any other path.
    static const QRegularExpression secret(QStringLiteral(
        "juice: (?:[A-Za-z0-9_]+\\.c:\\d+: )?Got (?:username|realm|nonce): "));
    return secret.match(line).hasMatch();
}

bool isLibraryNoise(const QString& line)
{
    // Once per packet or per STUN attribute; none says which pair was
    // checked or how it went. The juice ones are matched after "juice: ".
    static const char* const kJuice[] = {
        "Received datagram", "Sending datagram", "Receiving datagram", "No more ",
        "Entering poll", "Leaving poll", "poll interrupted", "Bookkeeping",
        "Setting Differentiated", "STUN message is", "Found STUN entry",
        "Reading ", "Writing ", "Found data",
        "Found even port", "Found requested transport", "Found don't fragment",
        "Found reservation", "Got priority",
        "STUN fingerprint check", "STUN message integrity", "Nonce has cookie",
        "Remote agent is", "Not a STUN message", "Finished reading", "STUN attribute",
        "Looking up agent", "Received ChannelData", "Forwarding", "Fairness limit",
        "Received application datagram", "Demultiplexing incoming", "Response has mapped",
        "Updating gathering status", "Updating ordered candidate pairs",
        "Added map entry", "Removed ", "Interrupting connections thread",
        "Received non-STUN datagram", "A remote candidate exists",
    };
    // Matched anywhere in the juice message.
    static const char* const kJuiceAnywhere[] = {
        "matching incoming transaction", "matching incoming address",
    };
    static const char* const kLibrary[] = {
        "Send size=", "Incoming size=", "SCTP try send", "SCTP sent size", "SCTP recv",
        "SCTP sender dry", "usrsctp: ", "RtpHeader", "RTCP ", "RTP packet", "FB: ",
        "Process data", "SRTP", "SRTCP", "Demultiplexing", "Processing notification",
        "Poll in event", "Poll out event", "Poll timeout event", "Entering poll",
        "Exiting poll", "Handle upcall", "Handle write", "Incoming DTLS packet",
        "TCP is idle", "Registering incoming callback",
    };
    const int juice = static_cast<int>(line.indexOf(QLatin1String("juice: ")));
    if (juice >= 0) {
        QStringView message = QStringView(line).mid(juice + 7);
        // "agent.c:1234: " before the message itself.
        static const QRegularExpression source(QStringLiteral("^[A-Za-z0-9_]+\\.c:\\d+: "));
        const QRegularExpressionMatch at = source.matchView(message);
        if (at.hasMatch()) {
            message = message.mid(at.capturedLength());
        }
        for (const char* noise : kJuice) {
            if (message.startsWith(QLatin1String(noise))) {
                return true;
            }
        }
        for (const char* noise : kJuiceAnywhere) {
            if (message.contains(QLatin1String(noise))) {
                return true;
            }
        }
        return false;
    }
    const int colon = static_cast<int>(line.indexOf(QLatin1String(": ")));
    const QStringView message = colon >= 0 ? QStringView(line).mid(colon + 2) : QStringView(line);
    for (const char* noise : kLibrary) {
        if (message.startsWith(QLatin1String(noise))) {
            return true;
        }
    }
    return false;
}

void logPath(const char* path, const QString& text)
{
    if (!enabled()) {
        return;
    }
    write(path, text);
}

void logSelectedPair(const char* path, rtc::PeerConnection& peer)
{
    if (!enabled()) {
        return;
    }
    rtc::Candidate local;
    rtc::Candidate remote;
    try {
        if (!peer.getSelectedCandidatePair(&local, &remote)) {
            write(path, QStringLiteral("selected pair: none"));
            return;
        }
    } catch (const std::exception&) {
        write(path, QStringLiteral("selected pair: unavailable"));
        return;
    }
    write(path, QStringLiteral("selected pair: local %1 | remote %2")
                    .arg(QString::fromStdString(local.candidate()),
                         QString::fromStdString(remote.candidate())));
}

} // namespace IceDiagnostics

} // namespace NereusSDR
