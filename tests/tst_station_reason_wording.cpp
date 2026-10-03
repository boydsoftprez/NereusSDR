// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_station_reason_wording.cpp  (NereusSDR)
// =================================================================
//
// iPhone app plan, Task 4b (R-IOS-01, R-R3-21): every reason the station
// sends an app is plain operator words. The phone shows a reason exactly as
// sent, so a reason worded for developers (a function or class name, a
// requirement or phase name such as "R4", or "session", "revision",
// "capability", "actuation" and the like) is a bug the operator reads.
//
// How the reasons are found: a source scan, not a table, because the
// reasons are written where each refusal is decided (the dispatcher, the
// server, the facades, the accessory and settings code, the media
// controller), and a table would be a second copy that can drift from them.
//
//   * Each file in kReasonSources is read with its comments removed and
//     adjacent literals joined. In a whole-file entry every string literal
//     with a space in it outside a log statement is a reason and is
//     checked, and so is every literal of any length in a reason position:
//     a sender's reason argument (emitResult, commandResult, sessionEnd,
//     authResult, settingsReject, dropPeer, sendRejected,
//     sendAllocationResult, rejectAllocation, and the reject, rejectDetail
//     and fail helpers) or the right-hand side of an assignment to a
//     reason (…reason, …refusal, m_lastError, m_lastActionError). A
//     reason of one word is not a sentence an operator can act on and
//     fails. What .arg() inserts into a reason is part of it: an inserted
//     literal is checked as words, and any other inserted expression must
//     be named in the entry's plainInserts. A literal that never reaches
//     an app is named in the entry's notReasons, by its start, with why.
//     In a function entry only the named functions' bodies are read
//     (RadioModel.cpp and the models mix the station's reasons with this
//     app's own text).
//   * The guard: every function in src/core and src/models whose name says
//     it words a reason (…Reason, …Refusal, …ForStation, …FromStation,
//     applyMirroredValue) or that writes one through a QString*
//     out-parameter (…reason, …refusal), and every file that sends a
//     reason to an app (commandResult, sessionEnd, authResult,
//     settingsReject, propertyResult, emitResult, dropPeer, sendRejected,
//     sendAllocationResult, rejectAllocation), must be scanned here or
//     named in kAppSideReasons with why. A new reason site fails until it
//     is placed, whatever file it is in.
//   * What the station actually sent: every reason in a station message of
//     the link's session fixtures (tests/data/link/v1/sessions) is checked
//     the same way.
//
// Machine-readable codes are not reasons and keep their spelling: the
// receiver audio context's reasons (client-disabled, receiver-limit, ...),
// displayBudgetReason, and the two display retire reasons windows already
// in use compare as they are ("slice removed", "slice stream binding
// changed"). The last two are plain as written and are scanned anyway.
//
//   2026-09-28  J.J. Boyd / KG4VCF  Slice control plan Task 2:
//                                    changeRefusal replaces sliceRefusal;
//                                    a listener's refusal words scanned.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-30  J.J. Boyd / KG4VCF  Level Cal 2: the dispatcher's active-
//                                    slice check forwards changeRefusal;
//                                    levelCalHostSlice is the desktop's own.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-10-01  J.J. Boyd / KG4VCF  TX diagnostics lane: the unkey event
//                                    lines (DaemonMediaController::
//                                    unkeyEventLines) are the Core's log
//                                    text, named exactly in
//                                    unkeyEventLogText(); a guard holds
//                                    each to that function and the function
//                                    to the log. AI-assisted via Anthropic
//                                    Claude Code.
//   2026-10-01  J.J. Boyd / KG4VCF  TX diagnostics lane, review round: the
//                                    guard also fails on a longer literal
//                                    starting with an exempt text, on any use
//                                    of the lines in logUnkeyStats past the
//                                    log, and on any mention of
//                                    unkeyEventLines (also in *.mm).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-30  J.J. Boyd / KG4VCF  TX rulings: a listener's refused
//                                    receive-level write forwards
//                                    listenerChangeReason.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-30  J.J. Boyd / KG4VCF  Core-slice take-over: the hand-off
//                                    check's forward names the taker.
//                                    AI-assisted via Anthropic Claude Code.
// =================================================================
// Modification history (NereusSDR):
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Task 4b (R-IOS-01,
//                                    R-R3-21): created. AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Part A fix wave (R-IOS-01,
//                                    R-R3-21): one-word reasons, .arg()
//                                    insertions and out-parameter writers
//                                    in any file. AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  Part A re-review (R-IOS-01, R-R3-21):
//                                    a reason passed on without words of
//                                    its own is named with its source;
//                                    forwarding sites; real minimums.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  Lane B takes integration (R-IOS-01,
//                                    R-R3-21): integration's accessory
//                                    settings, filter policy, RF-Kit reset,
//                                    off-network and TCI reason sites.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  R3 completion carry, review I1
//                                    (R-R3-21, R-R3-38, R-IOS-01): the
//                                    takeover and version reasons scanned in
//                                    SessionEndReasons.cpp.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  R3 completion Task 8 (R-R3-21): a
//                                    reason that calls the Core "the
//                                    station" fails; the ham sense stays.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  R-R3-49 / R-R3-47: the Tuner Genius's
//                                    antenna, operate and bypass reasons.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Task 14 (R-IOS-08):
//                                    pair.fail's reason (pairFail,
//                                    sendPairFail) is scanned like every
//                                    other; the console's pairing code
//                                    notice is not a reason.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-24: Part C fix wave: the pairing code is never printed
//               to standard output (the journal on a packaged Core). J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic Claude
//               Code.
//   2026-09-24  J.J. Boyd / KG4VCF  R-R3-49 (parity Task 1): the Core's
//                                    on-the-air refusal and the window's
//                                    transmit settings reason.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  R-R3-49 (parity Task 2): the transmit
//                                    settings' range refusals and the Tune
//                                    Power command's reasons.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  R-R3-49 (parity Task 3): the TX
//                                    profile commands' and the RADE vocoder
//                                    reset's reasons.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  iPhone app Task 71 (R-IOS-02): the
//                                    several-devices sentences are plain.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  iPhone app Task 73 (R-IOS-02): the
//                                    refusal for another device's slice,
//                                    and a saved slice that did not fit.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25: iPhone app plan Task 34 (R-IOS-02): the transmit refusals
//               (TxRefusal.cpp) and their forwards. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-25 - iPhone app plan, desktop remote transmit (R-IOS-13): the
//                refusal sent with txPermitted is named as a forward. J.J.
//                Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-25: iPhone app plan Task 39 (D14, R-IOS-13): txState's stop
//               texts (TransmitStateFacade.cpp). J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  iPhone app Task 74 (R-IOS-30): the
//                                    confirm step's reasons, notices,
//                                    chooser `why` and change label, with a
//                                    name embedded and set aside.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  R-IOS-27, R-IOS-06: slice.selectBand
//                                    relays onBandButtonClicked's refusal,
//                                    now scanned. AI-assisted via Anthropic
//                                    Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  R-IOS-27, R-IOS-06: addTnfFromStation
//                                    (notch.addAtSlice) scanned.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  Receiver and transmit gaps plan,
//                                    Task 16: receive only's reasons
//                                    (MoxController, RadioModel) scanned.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-26  J.J. Boyd / KG4VCF  Transmit group fix wave 2 (R-IOS-02):
//                                    the freeze and on-air refusals' new
//                                    forwards. AI-assisted via Anthropic
//                                    Claude Code.
//   2026-09-26  J.J. Boyd / KG4VCF  Task 77 fix wave, I3: the tuner's
//                                    on-air refusal forward. AI-assisted
//                                    via Anthropic Claude Code.
//   2026-09-26  J.J. Boyd / KG4VCF  Task 77 fix round 2: the autotune's
//                                    own refusals (beginTgxlAutotune) are
//                                    scanned and forwarded. AI-assisted
//                                    via Anthropic Claude Code.
//   2026-09-26  J.J. Boyd / KG4VCF  Task 77 fix round 3: pgxlSwitchRefusal
//                                    and tunerTuningReason scanned and
//                                    forwarded. AI-assisted via Anthropic
//                                    Claude Code.
//   2026-09-26  J.J. Boyd / KG4VCF  Task 77 fix round 4:
//                                    ampStillSwitchingReason scanned and
//                                    forwarded. AI-assisted via Anthropic
//                                    Claude Code.
//   2026-09-26  J.J. Boyd / KG4VCF  Parity Task 19 (R-IOS-25):
//                                    SpotSourceHost's refusals scanned and
//                                    its readOnlyReason forwarded. AI-assisted
//                                    via Anthropic Claude Code.
//   2026-09-27  J.J. Boyd / KG4VCF  iPhone plan Task 22 (R-IOS-26): the
//                                    freedv.* refusals scanned. AI-assisted
//                                    via Anthropic Claude Code.
//   2026-09-26  J.J. Boyd / KG4VCF  Parity Task 21 (R-IOS-18):
//                                    StationRadios' refusals scanned.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-26  J.J. Boyd / KG4VCF  D79 (R-IOS-11, R-R3-49): the Core's
//                                    unknown band plan refusal forwards.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-28  J.J. Boyd / KG4VCF  Addendum G-42: the Extended transmit
//                                    setting's refusal forwards, and the
//                                    window's transmitPermissionReason.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  Level Cal: the calibration run's
//                                    refusals and messages scanned, and the
//                                    window's reason for an older Core.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  txEq.setCurve's refusals scanned; the
//                                    This Core page's two device reasons
//                                    named on the app side. AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  cfc.setProfile's refusals scanned
//                                    (transmitSettingsVersion 15).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  The PA on-air gate's refusals scanned
//                                    (Setup description version 20).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  JJ's rule: a reason that says "yet"
//                                    fails (it promises a future).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-28  J.J. Boyd / KG4VCF  Slice control plan Task 4:
//                                    SliceAccessController.cpp scanned;
//                                    the hand-off and controlTaken words'
//                                    names and letter are plain inserts.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-28  J.J. Boyd / KG4VCF  Slice control plan Task 5:
//                                    SliceAccessMirror::listenerReason
//                                    scanned (the Core's listener words,
//                                    held back in a remote window);
//                                    RadioModel forwards it; the link's
//                                    sliceAccessUnavailableReason and
//                                    SliceModel's getter are app side.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  Slice control plan Task 9: the
//                                    notice to a closed slice's listeners
//                                    (listenedClosedReason), forwarded.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  Slice control plan Task 17: the kind
//                                    word sliceHolderWords inserts.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-30  J.J. Boyd / KG4VCF  Radio codec: RadioModel's
//                                    orionMicPanelUnavailableReason placed
//                                    beside lpfBypassUnavailableReason.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-30  J.J. Boyd / KG4VCF  Shared-input filters: RadioModel's
//                                    lowPassHoldReason and the
//                                    rxFilter0LowPassReason getter.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-30  J.J. Boyd / KG4VCF  lowPassHoldReason's broadcast-band
//                                    high-pass sentences (JJ's ruling).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-30  J.J. Boyd / KG4VCF  AlexController's NoFilterPins reason
//                                    names the slice (JJ's ruling).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-30  J.J. Boyd / KG4VCF  RADE reason: radeStartReason (a
//                                    slice's radeReason) is scanned.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-10-01  J.J. Boyd / KG4VCF  DaemonApp's radioChangeStoppedReason
//                                    is scanned.
//                                    AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>

#include "core/security/DeviceStore.h"
#include "OperatorWording.h"

using namespace NereusSDR;

namespace {

QString sourcePath(const QString& relative)
{
    return QDir(QStringLiteral(NEREUS_SOURCE_DIR)).filePath(relative);
}

bool identifierChar(QChar c)
{
    return c.isLetterOrNumber() || c == QLatin1Char('_');
}

// The end (one past the closing quote) of the string or character literal
// that opens at `i`.
qsizetype literalEnd(const QString& text, qsizetype i)
{
    const QChar quote = text.at(i);
    qsizetype j = i + 1;
    while (j < text.size() && text.at(j) != quote && text.at(j) != QLatin1Char('\n')) {
        j += text.at(j) == QLatin1Char('\\') ? 2 : 1;
    }
    return qMin(j + 1, text.size());
}

bool opensCharLiteral(const QString& text, qsizetype i)
{
    return text.at(i) == QLatin1Char('\'') && (i == 0 || !identifierChar(text.at(i - 1)));
}

// A source file's code with its comments blanked and adjacent string
// literals joined ("a" "b" -> "ab"), as the compiler reads it.
QString codeOf(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    const QString text = QString::fromUtf8(file.readAll());
    QString code;
    code.reserve(text.size());
    qsizetype i = 0;
    qsizetype lastLiteralEnd = -1;
    while (i < text.size()) {
        const QChar c = text.at(i);
        const QChar next = i + 1 < text.size() ? text.at(i + 1) : QChar();
        if (c == QLatin1Char('/') && next == QLatin1Char('/')) {
            while (i < text.size() && text.at(i) != QLatin1Char('\n')) {
                ++i;
            }
            code += QLatin1Char(' ');
            continue;
        }
        if (c == QLatin1Char('/') && next == QLatin1Char('*')) {
            const qsizetype end = text.indexOf(QStringLiteral("*/"), i + 2);
            i = end < 0 ? text.size() : end + 2;
            code += QLatin1Char(' ');
            continue;
        }
        if (c == QLatin1Char('"') || opensCharLiteral(text, i)) {
            const qsizetype end = literalEnd(text, i);
            QString literal = text.mid(i, end - i);
            if (c == QLatin1Char('"') && lastLiteralEnd >= 0
                && code.mid(lastLiteralEnd).trimmed().isEmpty()) {
                code.truncate(lastLiteralEnd - 1);
                literal.remove(0, 1);
            }
            code += literal;
            lastLiteralEnd = c == QLatin1Char('"') ? code.size() : -1;
            i = end;
            continue;
        }
        if (!c.isSpace()) {
            lastLiteralEnd = -1;
        }
        code += c;
        ++i;
    }
    return code;
}

// The statements of `code`, split at ';' and '}' outside literals.
QStringList statementsOf(const QString& code)
{
    QStringList statements;
    QString current;
    qsizetype i = 0;
    while (i < code.size()) {
        const QChar c = code.at(i);
        if (c == QLatin1Char('"') || opensCharLiteral(code, i)) {
            const qsizetype end = literalEnd(code, i);
            current += code.mid(i, end - i);
            i = end;
            continue;
        }
        if (c == QLatin1Char('{') && i + 1 < code.size() && code.at(i + 1) == QLatin1Char('}')) {
            // An empty brace pair (QString{}, a default {}) ends nothing.
            current += QStringLiteral("{}");
            i += 2;
            continue;
        }
        if (c == QLatin1Char(';') || c == QLatin1Char('}')) {
            statements.append(current);
            current.clear();
        } else {
            current += c;
        }
        ++i;
    }
    statements.append(current);
    return statements;
}

// The index one past the bracket that closes the one at `open`.
qsizetype closingOf(const QString& code, qsizetype open, QChar opener, QChar closer)
{
    int depth = 0;
    qsizetype i = open;
    while (i < code.size()) {
        const QChar c = code.at(i);
        if (c == QLatin1Char('"') || opensCharLiteral(code, i)) {
            i = literalEnd(code, i);
            continue;
        }
        if (c == opener) {
            ++depth;
        } else if (c == closer && --depth == 0) {
            return i + 1;
        }
        ++i;
    }
    return code.size();
}

// One reason as written: its text (escapes kept) and what each .arg()
// after it inserts, as the text inside the .arg( ).
struct ReasonText {
    QString text;
    QStringList inserts;
    bool positioned = false;  // Found in a reason position, not only by its space.
    bool forwarded = false;   // Not words: an expression passed on as a reason.
};

// The calls that send a reason, with the index of their reason argument:
// the messages (commandResult, sessionEnd, authResult, settingsReject), the
// senders that wrap them, and the refusal helpers of the scanned files
// (DspAssetService::reject/rejectDetail, the PureSignal facade's fail).
struct ReasonSender {
    const char* name;
    int index;
};

const QList<ReasonSender>& reasonSenders()
{
    static const QList<ReasonSender> senders{
        {"emitResult", 3},   {"commandResult", 3},        {"sessionEnd", 0},
        {"authResult", 1},   {"settingsReject", 3},       {"dropPeer", 1},
        {"sendRejected", 3}, {"sendAllocationResult", 4}, {"rejectAllocation", 3},
        {"reject", 0},       {"rejectDetail", 1},         {"fail", 0},
        // PureSignal's fail(reason) and media's fail(sliceId, stream,
        // reason) both carry operator text; the first media argument is an
        // identifier, not a refusal.
        {"fail", 2},
        // iPhone app Task 14: pair.fail and StationServer's sender for it.
        {"pairFail", 0},     {"sendPairFail", 1},
        // Text the station sends as a property value (propertyTextSources).
        {"connectionFailed", 0}, {"failIdentityAdmission", 1}, {"setLastLoadError", 0},
        {"setNnrLastError", 0}, {"publish", 1}, {"setReceiveLayoutRestoreStatus", 1},
    };
    return senders;
}

// `text` split at its top-level commas (outside brackets and literals).
QStringList splitTopLevel(const QString& text)
{
    QStringList parts;
    QString current;
    int depth = 0;
    qsizetype i = 0;
    while (i < text.size()) {
        const QChar c = text.at(i);
        if (c == QLatin1Char('"') || opensCharLiteral(text, i)) {
            const qsizetype end = literalEnd(text, i);
            current += text.mid(i, end - i);
            i = end;
            continue;
        }
        if (c == QLatin1Char('(') || c == QLatin1Char('{') || c == QLatin1Char('[')) {
            ++depth;
        } else if (c == QLatin1Char(')') || c == QLatin1Char('}') || c == QLatin1Char(']')) {
            --depth;
        } else if (c == QLatin1Char(',') && depth == 0) {
            parts.append(current);
            current.clear();
            ++i;
            continue;
        }
        current += c;
        ++i;
    }
    parts.append(current);
    return parts;
}

// The parts of `statement` that are a reason: each sender's reason
// argument, and the right-hand side of an assignment to a reason
// (…reason, …Reason, …refusal, m_lastError, m_lastActionError).
QStringList reasonExpressionsIn(const QString& statement)
{
    QStringList expressions;
    for (const ReasonSender& sender : reasonSenders()) {
        const QRegularExpression call(
            QStringLiteral("\\b%1\\s*\\(").arg(QLatin1String(sender.name)));
        QRegularExpressionMatchIterator it = call.globalMatch(statement);
        while (it.hasNext()) {
            const QRegularExpressionMatch match = it.next();
            const qsizetype open = match.capturedEnd() - 1;
            const qsizetype close = closingOf(statement, open, QLatin1Char('('), QLatin1Char(')'));
            const QStringList arguments =
                splitTopLevel(statement.mid(open + 1, close - open - 2));
            // These are two distinct, known helper signatures. Do not
            // interpret media's slice id as PureSignal's one-argument
            // refusal, or omit the actual third-argument media reason.
            if (QLatin1String(sender.name) == QLatin1String("fail")
                && ((sender.index == 0 && arguments.size() != 1)
                    || (sender.index == 2 && arguments.size() < 3))) {
                continue;
            }
            if (sender.index < arguments.size()) {
                expressions.append(arguments.at(sender.index));
            }
        }
    }
    static const QRegularExpression assignment(QStringLiteral(
        "\\b(?:\\w*[Rr]eason|\\w*[Rr]easonText|\\w*[Rr]efusal|m_lastError|m_lastActionError"
        "|m_error|m_settingsSaveError|m_lastListenError)\\s*=(?!=)"));
    const QRegularExpressionMatch assigned = assignment.match(statement);
    // A bool named for a reason (`const bool plainReason = ...`) is not one.
    static const QRegularExpression boolDeclaration(QStringLiteral("\\bbool\\s+$"));
    if (assigned.hasMatch()
        && !boolDeclaration.match(statement.left(assigned.capturedStart())).hasMatch()) {
        expressions.append(statement.mid(assigned.capturedEnd()));
    }
    return expressions;
}

// What the .arg() calls right after the literal ending at `end` insert:
// the text inside each .arg( ), whitespace simplified. A wrapper's closing
// bracket (QStringLiteral( ), tr( )) may come first.
QStringList insertsAfter(const QString& code, qsizetype end)
{
    QStringList inserts;
    qsizetype i = end;
    const auto skipSpace = [&code, &i]() {
        while (i < code.size() && code.at(i).isSpace()) {
            ++i;
        }
    };
    skipSpace();
    if (i < code.size() && code.at(i) == QLatin1Char(')')) {
        ++i;
    }
    for (;;) {
        skipSpace();
        if (!code.mid(i, 5).startsWith(QStringLiteral(".arg("))) {
            break;
        }
        const qsizetype open = i + 4;
        const qsizetype close = closingOf(code, open, QLatin1Char('('), QLatin1Char(')'));
        inserts.append(code.mid(open + 1, close - open - 2).simplified());
        i = close;
    }
    return inserts;
}

// Every reason written in `code`, outside log statements: each string
// literal with a space in it (as before), and each literal of any length
// in a reason position (reasonExpressionsIn), with what .arg() inserts.
QList<ReasonText> reasonsIn(const QString& code)
{
    static const QRegularExpression log(QStringLiteral(
        "\\bq(C(Warning|Info|Debug|Critical)|Warning|Debug|Info|Critical)\\b"));
    static const QRegularExpression literal(QStringLiteral("\"((?:[^\"\\\\\\n]|\\\\.)*)\""));
    QList<ReasonText> found;
    const auto add = [&found](const QString& text, const QStringList& inserts, bool positioned) {
        for (ReasonText& known : found) {
            if (known.text == text) {
                for (const QString& insert : inserts) {
                    if (!known.inserts.contains(insert)) {
                        known.inserts.append(insert);
                    }
                }
                known.positioned = known.positioned || positioned;
                return;
            }
        }
        found.append({text, inserts, positioned});
    };
    for (QString statement : statementsOf(code)) {
        const QRegularExpressionMatch logged = log.match(statement);
        if (logged.hasMatch()) {
            statement.truncate(logged.capturedStart());
        }
        QStringList positioned;
        static const QRegularExpression empty(
            QStringLiteral("^(?:QString\\(\\)|QString\\{\\}|\\{\\}|QStringLiteral\\(\\)|)$"));
        for (const QString& expression : reasonExpressionsIn(statement)) {
            // An expression with no literal in it passes on words written
            // elsewhere; it must be named where it is scanned.
            const QString bare = expression.simplified();
            // A sender's own declaration (`const QString& reason`) is a
            // parameter, not a call passing something on.
            static const QRegularExpression parameter(
                QStringLiteral("^(?:const\\s+)?QString\\s*&?\\s*\\w+(?:\\s*=.*)?$"));
            if (parameter.match(bare).hasMatch()) {
                continue;
            }
            if (!bare.contains(QLatin1Char('"')) && !empty.match(bare).hasMatch()) {
                const bool known = std::any_of(found.cbegin(), found.cend(),
                    [&bare](const ReasonText& r) { return r.forwarded && r.text == bare; });
                if (!known) {
                    found.append({bare, {}, true, true});
                }
                continue;
            }
            QRegularExpressionMatchIterator it = literal.globalMatch(expression);
            while (it.hasNext()) {
                const QRegularExpressionMatch match = it.next();
                // A conditional reason can compare a wire verb before
                // choosing its text. That comparison operand is a code,
                // not a reason; the selected arm is still scanned.
                static const QRegularExpression comparedOperand(QStringLiteral("(?:==|!=)\\s*$"));
                if (comparedOperand.match(expression.left(match.capturedStart())).hasMatch()) {
                    continue;
                }
                const QString text = match.captured(1);
                if (!text.isEmpty()) {
                    positioned.append(text);
                }
            }
        }
        QRegularExpressionMatchIterator it = literal.globalMatch(statement);
        while (it.hasNext()) {
            const QRegularExpressionMatch match = it.next();
            const QString text = match.captured(1);
            const bool inPosition = positioned.contains(text);
            if (text.contains(QLatin1Char(' ')) || inPosition) {
                add(text, insertsAfter(statement, match.capturedEnd()), inPosition);
            }
        }
    }
    return found;
}

struct FunctionBody {
    QString name;
    QString body;
    QString params;  // The text inside its parameter list's brackets.
};

// Every function defined in `code` (a body follows its parameter list)
// whose name matches `names`, with its body.
QList<FunctionBody> functionsIn(const QString& code, const QRegularExpression& names)
{
    static const QRegularExpression qualifiers(
        QStringLiteral("^\\s*(?:(?:const|noexcept|override|final)\\b\\s*)*"));
    QList<FunctionBody> found;
    static const QRegularExpression definition(
        QStringLiteral("\\b(?:[A-Za-z_][A-Za-z0-9_]*::)*([A-Za-z_][A-Za-z0-9_]*)\\s*\\("));
    QRegularExpressionMatchIterator it = definition.globalMatch(code);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        const QString name = match.captured(1);
        if (!names.match(name).hasMatch()) {
            continue;
        }
        const qsizetype afterParams = closingOf(code, match.capturedEnd() - 1,
                                                QLatin1Char('('), QLatin1Char(')'));
        const QRegularExpressionMatch tail = qualifiers.match(code.mid(afterParams, 64));
        const qsizetype brace = afterParams + tail.capturedLength();
        if (brace >= code.size() || code.at(brace) != QLatin1Char('{')) {
            continue;
        }
        const qsizetype end = closingOf(code, brace, QLatin1Char('{'), QLatin1Char('}'));
        const qsizetype paramsStart = match.capturedEnd();
        found.append({name, code.mid(brace, end - brace),
                      code.mid(paramsStart, afterParams - 1 - paramsStart)});
    }
    return found;
}

// Developer wording beyond the internal terms: identifiers (camelCase,
// Class::member, call()), dotted verb names, requirement and phase names,
// and words that describe the code rather than what happened.
QString developerWordingIn(const QString& text)
{
    static const QRegularExpression names(QStringLiteral(
        "\\b(?!dB\\b|dBm\\b|kHz\\b)[a-z]+[A-Z][A-Za-z0-9]*\\b|::|\\w\\(\\)|\\b[a-z]+\\.[a-z][A-Za-z0-9]+\\b"
        "|\\bR\\d+\\b|\\bPhase \\d|\\bTask \\d|\\bPS3\\b|\\b[TP]GXL\\b"));
    static const QRegularExpression words(QStringLiteral(
        "\\bactuat|\\bargument|\\bdaemon|\\bhook\\b|\\binbound|\\boutbound|\\bschema"
        "|\\ballocator|\\bcohost|\\bmirror|\\bidentit|\\bmalformed|\\bverb\\b|-scoped\\b"),
        QRegularExpression::CaseInsensitiveOption);
    for (const QRegularExpression* pattern : {&names, &words}) {
        const QRegularExpressionMatch match = pattern->match(text);
        if (match.hasMatch()) {
            return match.captured(0);
        }
    }
    return {};
}

// Why `text` is not plain operator words, or an empty string. A reason
// calls the Core "the Core", never "the station" (R-R3-21): only the ham
// sense of "station" (the station's amplifier, your station) is kept.
QString wordingProblemIn(const QString& text)
{
    if (text.trimmed().isEmpty()) {
        return QStringLiteral("empty");
    }
    const QString term = OperatorWording::internalTermIn(text);
    if (!term.isEmpty()) {
        return term;
    }
    const QString developer = developerWordingIn(text);
    if (!developer.isEmpty()) {
        return developer;
    }
    // JJ's rule for what the phone shows (2026-09-29): "yet" promises a
    // future the Core cannot keep; a reason says what is true now.
    static const QRegularExpression promise(QStringLiteral("\\byet\\b"),
                                            QRegularExpression::CaseInsensitiveOption);
    if (promise.match(text).hasMatch()) {
        return QStringLiteral("yet (say what is true now)");
    }
    const QString station = OperatorWording::coreCalledStationIn(text);
    return station.isEmpty() ? QString() : station + QStringLiteral(" (say the Core)");
}

// Why the words .arg() inserts into a reason are not plain, or an empty
// string. A literal is checked as words (one word is fine: a band, a
// number's unit); anything else must be named in `plainInserts`.
QString insertProblemIn(const QString& insert, const QStringList& plainInserts)
{
    static const QRegularExpression literalOnly(QStringLiteral(
        "^(?:QStringLiteral|QLatin1String|QString|tr)?\\s*\\(?\\s*\"((?:[^\"\\\\]|\\\\.)*)\"\\s*\\)?$"));
    const QRegularExpressionMatch literal = literalOnly.match(insert);
    if (literal.hasMatch()) {
        const QString words = literal.captured(1);
        const QString term = OperatorWording::internalTermIn(words);
        const QString problem = term.isEmpty() ? developerWordingIn(words) : term;
        return problem.isEmpty()
                   ? QString()
                   : QStringLiteral("inserts \"%1\" [%2]").arg(words, problem);
    }
    return plainInserts.contains(insert)
               ? QString()
               : QStringLiteral("inserts %1, which is not known to be plain words").arg(insert);
}

// Every wording problem of one reason: its own words, one word standing
// alone (not a sentence an operator can act on), and what .arg() inserts.
QStringList problemsOf(const ReasonText& reason, const QStringList& plainInserts,
                       const QStringList& forwards = {})
{
    QStringList problems;
    if (reason.forwarded) {
        if (!forwards.contains(reason.text)) {
            problems.append(QStringLiteral("passes on %1 as a reason; name it in forwards with "
                                           "where its words come from")
                                .arg(reason.text));
        }
        return problems;
    }
    QString problem = wordingProblemIn(reason.text);
    static const QRegularExpression letter(QStringLiteral("[A-Za-z]"));
    if (problem.isEmpty() && reason.positioned && letter.match(reason.text).hasMatch()
        && !reason.text.trimmed().contains(QLatin1Char(' '))) {
        problem = QStringLiteral("one word");
    }
    if (!problem.isEmpty()) {
        problems.append(QStringLiteral("\"%1\" [%2]").arg(reason.text, problem));
    }
    for (const QString& insert : reason.inserts) {
        const QString inserted = insertProblemIn(insert, plainInserts);
        if (!inserted.isEmpty()) {
            problems.append(QStringLiteral("\"%1\" %2").arg(reason.text, inserted));
        }
    }
    return problems;
}

// Every wording problem among the reasons written in `code`. `plainInserts`
// names the .arg() arguments, by their text, known to insert plain words.
QStringList wordingProblemsInCode(const QString& code, const QStringList& plainInserts)
{
    QStringList problems;
    for (const ReasonText& reason : reasonsIn(code)) {
        problems.append(problemsOf(reason, plainInserts));
    }
    return problems;
}

struct ReasonSource {
    const char* file;
    QStringList functions;   // Empty: the whole file.
    QStringList notReasons;  // Literals here that never reach an app, by their start.
    int atLeast;             // So the scan cannot pass on nothing.
    // .arg() arguments here, by their text inside .arg( ), that insert
    // plain words (numbers, a band, an address, the operator's own label).
    QStringList plainInserts = {};
    // Reason expressions here with no words of their own (a variable, a
    // call), by their text, each passing on words scanned where they are
    // written (named with where, beside the entry).
    QStringList forwards = {};
    // Only literals in a reason position count: the file also writes
    // device commands with spaces in them (the accessory connections).
    bool positionedOnly = false;
};

// TX diagnostics lane: the literals of DaemonMediaController::
// unkeyEventLines, whole, as the scan reads them (adjacent literals
// joined). They are the Core's log text: logUnkeyStats logs each line it
// returns with qCInfo and nothing else reads them (unkeyEventLinesAreLogOnly
// holds both). Each entry is one whole literal of that function, as
// written: a few are a line's opening, most are a fragment appended to it
// (", silent %1 ms"). notReasons matches by start, so the guard fails on
// any literal in the file that starts with one of these texts unless it is
// that exact literal, inside unkeyEventLines.
const QStringList& unkeyEventLogText()
{
    static const QStringList text{
        QStringLiteral("Transmit ended (%1): "),
        QStringLiteral("microphone underrun %1 at +%2 ms of the line"),
        QStringLiteral(", RF start not measured"),
        QStringLiteral(", RF never started"),
        QStringLiteral(" and +%1 ms of RF"),
        QStringLiteral(", %1 ms before RF started"),
        QStringLiteral(", silent %1 ms"),
        QStringLiteral(", still silent at unkey"),
        QStringLiteral("; largest gap between packets %1 ms"),
        QStringLiteral("; no gap between packets measured"),
        QStringLiteral(", latest packet %1 ms behind its timestamp"),
        QStringLiteral(", no packet timestamps measured"),
        QStringLiteral("radio ran dry at +%1 ms of the key, after a send gap of %2 ms"),
        QStringLiteral("catch-up burst %1 at +%2 ms of the key, %3 frames after a send gap of "
                       "%4 ms"),
    };
    return text;
}

const QList<ReasonSource>& reasonSources()
{
    static const QList<ReasonSource> sources{
        // Shared radio-mic refusals reach Core replies as well as the window.
        {"src/core/session/RemoteMicSource.h",
         {QStringLiteral("remoteRadioVoxReason"), QStringLiteral("remoteRadioProgramReason"),
          QStringLiteral("remoteMicLegacyReason")}, {}, 3, {}, {}},
        // Shared-input filters, ruling (d): rxFilter0LowPassReason. The
        // slices are named by letter and band ("B on 20m"), one or several
        // joined by joinRangeNames. On the HL2 it also names the slices the
        // N2ADR broadcast-band high-pass is off for (JJ's ruling of
        // 2026-09-30).
        {"src/models/RadioModel.cpp", {QStringLiteral("lowPassHoldReason")},
         {QStringLiteral("%1 on %2")}, 5,
         {QStringLiteral("named(top)"), QStringLiteral("lower.first()"),
          QStringLiteral("joinRangeNames(lower)"), QStringLiteral("off.first()"),
          QStringLiteral("joinRangeNames(off)")}},
        // RADE reason: a slice's radeReason, why a RADE slice has no
        // working decoder. The slices are named by letter.
        {"src/models/RadioModel.cpp", {QStringLiteral("radeStartReason")}, {}, 6,
         {QStringLiteral("letter"),
          QStringLiteral("letter, receiverLetter(*m_restoredRadeReceiveOwner)")}},
        // session.end, auth.result, property.result, settings.reject and
        // the refusals of a verb an older app sends.
        {"src/core/session/StationServer.cpp", {},
         {// start(): the Core's own setup error and the WebSocket server's
          // name, and the pairing banner nereusd prints on its console.
          "Qt reports no working TLS backend", "No authentication token available",
          "NereusSDR station", "\\n  ====="},
         30,
         // The other app's network address (WebSocketTransport::peerDescription),
         // and (iPhone app Task 73) the name of the device a slice belongs
         // to, the operator's own word (ruling 4.3).
         {QStringLiteral("description"), QStringLiteral("owner"),
          // DeviceStore validates the replacement device's own name; the
          // takeover sentence uses describe(selection.deviceId) or the
          // registry's saved selection.name, both operator labels.
          QStringLiteral("takerName"),
          // R-R3-49: a Watt Meter calibration point's maximum, a number of
          // watts (calibrationKeyValueRefusal).
          QStringLiteral("spec.maximum"),
          // Review C1: alexLpfKeyValueRefusal's band name (160m to 6m),
          // "start" or "end", and its two limits in MHz ("Choose the %1
          // low-pass %2 from %3 to %4 MHz."), and the same band and edge
          // in its refusal of a key written in another case.
          QStringLiteral("band, edgeName, lowest, highest"),
          QStringLiteral("band, edgeName"),
          // Slice control plan Task 2: listenerChangeReason's slice letter
          // (A to P); the controller's name beside it is `owner` above.
          QStringLiteral("letter"),
          // Slice control plan Task 4: the controller's or the taker's
          // name (the operator's own words, ruling 4.3) and the letter.
          QStringLiteral("name, letter"), QStringLiteral("planDevice(taker).name, letter"),
          // Slice control plan Task 17: sliceHolderWords' kind word for a
          // device with no name (DeviceSessionRegistry::kindWord, lowercase).
          QStringLiteral("kind")},
         {// listen(): the Core's own setup error (m_lastError), for its
          // console and log; never sent to an app.
          QStringLiteral("CertificateStore::tlsBackendDiagnostic()"),
          QStringLiteral("m_certificates->lastError()"),
          QStringLiteral("m_openingGate->errorString()"),
          // dropPeer's and sendRejected's parameter, from this file's calls.
          QStringLiteral("reason"),
          // iPhone app Task 76: a display budget share's reason, a code
          // (DisplayBudgetReason, the link's section 6.4), not words.
          QStringLiteral("peer.budgetShareReason"), QStringLiteral("share.reason"),
          QStringLiteral("DisplayBudgetReason::None"),
          QStringLiteral("self->budgetShareReason"),
          QStringLiteral("peerHoldsSessions(transport) ? reason "
                         ": displayBudgetReasonForOlderDevice(reason)"),
          // SessionEndReasons' version and takeover reasons, scanned below.
          QStringLiteral("SessionEndReasons::versionRefused(m_supportedMajors, "
                         "message.supportedMajors)"),
          QStringLiteral("SessionEndReasons::takenOver(description)"),
          // Each model's readOnlyReason, scanned below.
          QStringLiteral("IoBoardHl2Facade::readOnlyReason()"),
          QStringLiteral("AmplifierModel::readOnlyReason()"),
          QStringLiteral("RfKitModel::readOnlyReason()"),
          QStringLiteral("StationTciModel::readOnlyReason()"),
          // Parity Task 19: the `spotSources` object's, scanned below.
          QStringLiteral("SpotSourceHost::readOnlyReason()"),
          // Fix wave after parity Tasks 19 and 21 (I5): the radio verbs'
          // refusal to a token sign-in, scanned in StationRadios.cpp.
          QStringLiteral("StationRadios::pairedDeviceReason()"),
          QStringLiteral("AccessoryDataModel::readOnlyReason()"),
          QStringLiteral("AccessorySettingsModel::readOnlyReason()"),
          // Literals inserted into `refusals` in this file.
          QStringLiteral("refusals.value(update.name)"),
          // The attenuator and antenna facades' settle reasons, scanned below.
          QStringLiteral("m_radioModel->stepAttFacade()->settleReason(update.name)"),
          QStringLiteral("m_radioModel->alexAntennaFacade()->settleReason(update.name)"),
          // A constant of this file, its literal scanned here.
          QStringLiteral("QString::fromLatin1(kReceiveOnlyTransmitReason)"),
          // iPhone app Task 71: constants of this file, their literals
          // scanned here.
          QStringLiteral("QString::fromLatin1(kCoreFullReason)"),
          QStringLiteral("QString::fromLatin1(kOlderWindowCoreFullReason)"),
          QStringLiteral("QString::fromLatin1(kSameDeviceReason)"),
          QStringLiteral("QString::fromLatin1(kLeftReason)"),
          // iPhone app Task 74: ruling 10.2's refusal at admission,
          // StationReceivers.cpp's olderWindowWithoutSliceReason, scanned
          // there.
          QStringLiteral("noSlice"),
          // iPhone app Task 73: functions of this file, their literals
          // scanned here (the owner's name is the operator's own word,
          // ruling 4.3).
          QStringLiteral("changeRefusal(requester, sliceId)"),
          QStringLiteral("ownedElsewhereReason(sliceId)"),
          // TX rulings: a listener's receive-level write (ATT, preamp,
          // auto-att) on the slice it hears; listenerChangeReason's words,
          // scanned here.
          QStringLiteral("listenerChangeReason(shown)"),
          // Fix wave I3: a slice's settings key; ownedElsewhereReason's
          // words, scanned here.
          QStringLiteral("sliceSettingsRefusal(transport, key)"),
          // R-R3-49 / R-IOS-18: the PA profile verbs' gate, its literals
          // scanned here (and kReceiveOnlyTransmitReason's).
          QStringLiteral("paProfileRefusal(transport)"),
          // StateMirror's and SettingsProxyServer's results, scanned below.
          QStringLiteral("result.reason"),
          QStringLiteral("m_settingsServer->otherRadioRefusal(key)"),
          QStringLiteral("refusal"),
          // R-R3-49 (parity Task 1): RadioModel::stationOnAirRefusal's
          // reason (RadioModel::onAirReason, scanned below).
          QStringLiteral("onAir"),
          // A code windows compare, not a reason (section 17).
          QStringLiteral("m_displayBudgetReason"),
          // iPhone app plan Task 34: the transmit refusals' sentences,
          // written in src/core/safety/TxRefusal.cpp (scanned below).
          QStringLiteral("m_transmitHolder->keyRefusalFor(requester)"),
          QStringLiteral("TxRefusals::appCannotTransmit().text"),
          // A TxRefusal value is first stored then its text sent. Its
          // source sentence is scanned in TxRefusal.cpp.
          QStringLiteral("TxRefusals::appCannotTransmit()"),
          QStringLiteral("refusal.text"),
          QStringLiteral("decision.refusal.text"),
          // Radio input admission and its synchronous recheck forward the
          // canonical TxRefusal.cpp sentences, including holder refusals.
          QStringLiteral("TxRefusals::notReady().text"),
          QStringLiteral("decision.refusal"),
          QStringLiteral("!now.permitted ? now.refusal : m_transmitHolder ? "
                         "m_transmitHolder->keyRefusalFor(requester) : TxRefusal{}"),
          // The shared inline sentence is scanned in RemoteMicSource.h above.
          QStringLiteral("remoteMicLegacyReason()"),
          // Addendum G-42: the Extended transmit setting's refusal, which
          // returns the transmit gate's sentence (TxRefusal.cpp), the
          // on-air sentence (RadioModel::onAirReason) or its own literal
          // words in StationServer.cpp, each scanned.
          QStringLiteral("transmitGateSettingRefusal(transport, key, &value)"),
          QStringLiteral("transmitGateSettingRefusal(transport, key, nullptr)"),
          // R-R3-49 / R-IOS-27: a PA key's on-air refusal
          // (paSettingOnAirRefusalFor), RadioModel::paSettingOnAirRefusal's
          // words, scanned in RadioModel.cpp's entry.
          QStringLiteral("pa"),
          // Desktop remote transmit: the same sentences, sent with
          // txPermitted in capabilities (txRefusalReason) and remembered.
          QStringLiteral("txRefusalOf(caps)"),
          QStringLiteral("txDecision.refusal.text"),
          // Fix wave I2: the freeze's refusal (TxRefusal.cpp's words).
          QStringLiteral("frozen.text"), QStringLiteral("stationFreezeRefusal(sliceId).text"),
          // Fix wave 2: the same freeze asked of a whole message.
          QStringLiteral("freezeRefusalFor(message).text"),
          // R-R3-49 (parity Task 5): powerPageKeyValueRefusal's literals,
          // this file's own, scanned here.
          QStringLiteral("range"),
          // D79 (R-IOS-11): bandPlanRefusal's literal, this file's own,
          // scanned here.
          QStringLiteral("plan"),
          // These functions' own sentences are scanned in this file;
          // transmitSettingOnAirRefusal also relays RadioModel::onAirReason.
          QStringLiteral("transmitSettingOnAirRefusal(key)"),
          QStringLiteral("bandPlanRefusal(key, message.updates.first().value)"),
          // Antenna admission forwards the server helper, whose own words
          // and the dispatcher's returned words are scanned in these entries.
          QStringLiteral("radioAntennaRowRefusal(transport, message)"),
          QStringLiteral("antennaRefusal")}},
        // iPhone app Task 74 (R-IOS-30): the confirm step's answers and
        // refusals, confirm.request and notice reasons, and the chooser's
        // `why`. Device names inserted are the operator's own words (ruling
        // 4.3); slice letters, counts and bands are plain.
        // iPhone app plan Task 77: taking transmit's refusals and the
        // transmitTaken notice's words. The taker's name is the operator's
        // own words or "Radio" (ruling 4.3, ruling 8.1).
        // Slice control plan Task 4: listen, stop listening, take control
        // and release. Only the slice letter is inserted.
        {"src/core/session/SliceAccessController.cpp", {}, {}, 8,
         {QStringLiteral("letter"), QStringLiteral("letterOf(sliceId)")},
         {// refused()'s parameter, from this file's calls.
          QStringLiteral("reason"),
          // StationServer::handOffRefusal's words (scanned there).
          QStringLiteral("m_hooks.cannotHandOff(former, device, sliceId)")}},
        {"src/core/session/StationTransmitTake.cpp", {}, {}, 2,
         {QStringLiteral("takerName")},
         {// This file's own constant and reason function, scanned here;
          // the refusals are TxRefusal.cpp's (scanned there).
          QStringLiteral("QString::fromLatin1(kWaitingReason)"),
          QStringLiteral("takenReason(takerName)"), QStringLiteral("refusal.text"),
          // The session's own transmit gate (StationTxGate, TxRefusal.cpp's
          // words).
          QStringLiteral("sessionTransmitRefusal(transport)")}},
        {"src/core/session/StationReceivers.cpp", {}, {}, 12,
         {QStringLiteral("anchorName"), QStringLiteral("names"), QStringLiteral("takerName"),
          QStringLiteral("name, letterWords"), QStringLiteral("number"),
          QStringLiteral("notRestored.size()"),
          // Fix wave: a count of slices, and notRestoredSentence's own
          // sentence, scanned here.
          QStringLiteral("count"), QStringLiteral("notRestoredSentence(notRestored.size())")},
         {// Constants of this file and its reason functions, their
          // literals scanned here.
          QStringLiteral("notRestoredSentence(notRestored.size())"),
          QStringLiteral("QString::fromLatin1(kWaitingReason)"),
          QStringLiteral("QString::fromLatin1(kNoQuestionReason)"),
          QStringLiteral("QString::fromLatin1(kNoChoiceReason)"),
          QStringLiteral("QString::fromLatin1(kChangedReason)"),
          QStringLiteral("QString::fromLatin1(kNoTakeBackReason)"),
          QStringLiteral("QString::fromLatin1(kCentreRefusedReason)"),
          QStringLiteral("moved ? QString() : QString::fromLatin1(kCentreRefusedReason)"),
          QStringLiteral("affected.isEmpty() ? QString::fromLatin1(kChangedReason) : QString()"),
          QStringLiteral("pinReason(planDevice(anchor).name)"),
          // Slice control plan Task 8: the same, the anchor not connected.
          QStringLiteral("awayPinReason(planDevice(anchor).name)"),
          QStringLiteral("olderWindowAskReason(namesOf(check.named))"),
          QStringLiteral("takenOverReason(planDevice(taker).name)"),
          QStringLiteral("sliceMovedReason(planDevice(requester).name, letters)"),
          QStringLiteral("sliceClosedReason(planDevice(requester).name, lettersOf(it.value()))"),
          // Slice control plan Task 9: a closed slice's listeners are told;
          // listenedClosedReason's literals, scanned here.
          QStringLiteral("listenedClosedReason(why, name, lettersOf(it.value()))"),
          QStringLiteral("receiver ? receiverTakenReason(takerName, letters) : "
                         "sliceTakenReason(takerName, letters)"),
          // Fix wave 2: the on-air and freeze refusals (TxRefusal.cpp's
          // words, scanned there).
          QStringLiteral("check.onAir.text"), QStringLiteral("frozen.text"),
          QStringLiteral("freezeRefusalFor(question.original)"),
          QStringLiteral("stationFreezeRefusal(id)"),
          QStringLiteral("touches ? onAirWords(*holder) : TxRefusal{}"),
          // StationServer.cpp's slice refusal, scanned there.
          QStringLiteral("changeRefusal(requester, sliceId)"), QStringLiteral("refusal"),
          // Today's refusals from the model, the allocator and the
          // dispatcher (scanned where they are written), with the holders'
          // names appended in this file's own sentence (holdersSentence).
          QStringLiteral("withHolderNames(QString::fromLatin1(kPanNeedsReceiverReason), requester)"),
          QStringLiteral("withHolderNames(result.reason, requester)"),
          QStringLiteral("withHolderNames(r.reason, requester)"),
          QStringLiteral("result.reason"), QStringLiteral("r.reason"), QStringLiteral("reason"),
          // Take-back preflight passes ReceiverPlanner::RestorePlacement's
          // reason through unchanged. ReceiverPlanner and the allocator it
          // forwards are both scanned below.
          QStringLiteral("placement.reason")}},
        // Receiver choices and preflight refusals, including the complete
        // saved-slice restore check used before a take-back closes victims.
        {"src/core/session/ReceiverPlanner.cpp", {}, {}, 10,
         {// A slice letter, or a list of those letters.
          QStringLiteral("letters.first()"), QStringLiteral("joinWords(letters)")},
         {// planAddAfterClosing and planRestoreAfterClosing relay the
          // allocator's refusal; SliceStreamAllocator.cpp is scanned below.
          QStringLiteral("placement.reason")}},
        // iPhone app Task 75 (R-IOS-30): settings that affect every device:
        // the refusals, the `change` words of confirm.request and notice,
        // and the notice reasons. Device names inserted are the operator's
        // own words (ruling 4.3); values, units, bands, antennas and slice
        // letters are plain.
        {"src/core/session/StationSharedSettings.cpp", {},
         // The keys of `change` read back for the notice.
         {QStringLiteral("label"), QStringLiteral("from"), QStringLiteral("to")}, 10,
         {// The holder's and devices' names (the operator's own words); the
          // change's own label, from and to (this file's words, a band, a
          // value with its unit, or the operator's own setting); slice
          // letters; an antenna's name; numbers.
          QStringLiteral("holderShortName"), QStringLiteral("name, label, from, to"),
          QStringLiteral("ReceiverPlanner::joinWords(letters)"),
          QStringLiteral("antenna, names.first()"),
          QStringLiteral("antenna, ReceiverPlanner::joinWords(names)"),
          QStringLiteral("number"),
          QStringLiteral("QString::number(centreHz / 1e6, 'f', 6), "
                         "QString::number(std::lround(widthHz))"),
          QStringLiteral("adc"), QStringLiteral("where"), QStringLiteral("what, group"),
          QStringLiteral("hz / 1000"), QStringLiteral("receiverWords(reach.stream)"),
          QStringLiteral("adcWords(chain)"),
          QStringLiteral("QString::number(notch->centerHz / 1e6, 'f', 6)")},
         {// This file's reason functions and StationReceivers.cpp's older
          // window sentence, their literals scanned where written; the
          // settings proxy's refusal (scanned there) and this file's.
          QStringLiteral("onAirReason(planDevice(topology.transmit.holder).shortName)"),
          // Merge of the trunk into the transmit lane: Task 34's on-air
          // refusal (TxRefusal.cpp's words, scanned there).
          QStringLiteral("onAir.text"), QStringLiteral("refused.text"), QStringLiteral("onAir"),
          // StationTxGate::decide returns TxRefusal.cpp's scanned text.
          QStringLiteral("decision.refusal.text"),
          QStringLiteral("olderWindowReason(ReceiverPlanner::joinWords(names))"),
          QStringLiteral("QString::fromLatin1(kTargetChangedReason)"),
          QStringLiteral("antennaKeptReason(antenna, names)"), QStringLiteral("reason"),
          QStringLiteral("refusal"), QStringLiteral("applied ? QString() : refusal"),
          // A deferred rate change's own result (the dispatcher's, scanned
          // there).
          QStringLiteral("result.reason"),
          // Confirmed antenna changes forward the same scanned server helper.
          QStringLiteral("radioAntennaRowRefusal(transport, question.original)"),
          QStringLiteral("antennaRefusal")}},
        // command.result for every verb.
        // The device's name ("Power Genius", "Tuner Genius") and what the
        // request asked, both this file's own literals
        // (handleAccessoryDeviceSettings).
        {"src/core/session/SessionCommandDispatcher.cpp", {}, {}, 30,
         {QStringLiteral("device"), QStringLiteral("what")},
         {// Out-parameters and results of the scanned model, facade and
          // allocator functions each verb calls (RadioModel, the PureSignal
          // facade, SliceModel, AlexAntennaFacade, DspAssetService,
          // SliceStreamAllocator), and emitResult's own parameter.
          QStringLiteral("reason"), QStringLiteral("result.reason"),
          QStringLiteral("refusal"), QStringLiteral("facade->lastActionError()"),
          QStringLiteral("slice->nnrLastError()"), QStringLiteral("accepted ? QString() : reason"),
          QStringLiteral("rejectionReason"), QStringLiteral("outcome.reason"),
          // slice.selectBand: RadioModel::onBandButtonClicked's refusal,
          // scanned in RadioModel.cpp's entry.
          QStringLiteral("ignoredReason"),
          QStringLiteral("receiveOnly ? alex->setRxOnlyAntForBand(Band(band), antenna) : "
                         "alex->setRxAntForBand(Band(band), antenna)"),
          // A function of this file, its literal scanned here.
          QStringLiteral("notRepresentableReason()"),
          // Level Cal: RadioModel::requestStartLevelCalibration's refusal,
          // worded in LevelCalibrationService.cpp and LevelCalibrationRun.cpp
          // (both scanned below) or RadioModel.cpp's on-air words.
          QStringLiteral("m_radioModel->requestStartLevelCalibration( "
                         "static_cast<float>(levelDbm), frequencyHz, sliceId)"),
          // iPhone app Task 73: StationServer::changeRefusal (slice control plan
          // Task 2; was sliceRefusal), scanned there.
          QStringLiteral("m_sliceAccess(m_requester, sliceId)"),
          // Level Cal 2: Start with no slice named checks the Core's active
          // slice the same way (StationServer::changeRefusal, scanned there).
          QStringLiteral("active != nullptr ? m_sliceAccess(m_requester, active->sliceIndex()) : "
                         "QString()"),
          // Fix wave 2: the same, re-run when a rate change is applied, and
          // StationSharedSettings' kTargetChangedReason, handed over with
          // the rate change's closes (scanned there).
          QStringLiteral("access(requester, sliceId)"),
          QStringLiteral("std::exchange(m_rateChangedReason, {})"),
          // AlexAntennaFacade's filter policy refusal, scanned below.
          QStringLiteral("alex->setBpfModeForChain(chain, mode)"),
          // iPhone app plan Task 34: the transmit refusals, worded in
          // src/core/safety/TxRefusal.cpp (scanned below) and handed in by
          // StationServer's transmit access.
          QStringLiteral("refusal.text"), QStringLiteral("result.refusal.text"),
          QStringLiteral("m_transmitAccess.onAir(m_requester)"),
          // StationServer supplies this callback; it returns a TxRefusal
          // from StationTxGate/TxRefusal.cpp, then emitRefusal sends text.
          QStringLiteral("m_transmitAccess.accessory(m_requester)"),
          // Fix wave M2: a release refused (TxRefusal.cpp's words).
          QStringLiteral("m_transmitAccess.release(m_requester)"),
          // iPhone app plan Task 77 (ruling 7.7): the transmitter's
          // settings held by the holder (TxRefusal.cpp's words).
          QStringLiteral("m_transmitAccess.transmitter(m_requester)"),
          QStringLiteral("m_transmitAccess.txSlice(m_requester)"),
          // Parity mini-round: its TX antenna refusal (setAlexTxAntenna),
          // scanned below.
          QStringLiteral("alex->setTxAntForBand(Band(band), antenna)"),
          // R-R3-49 (parity Task 7): RadioModel::stationOnAirRefusal's
          // reason for a PureSignal arming verb (RadioModel::onAirReason,
          // scanned with StationServer).
          QStringLiteral("onAir"),
          // R-R3-46 (parity Task 14): RadioModel::requestIoBoardI2c's
          // refusal (scanned in RadioModel.cpp's entry), on a later turn.
          QStringLiteral("ok ? QString() : reason"),
          // This file's radio-antenna helper returns its scanned literals.
          QStringLiteral("radioAntennaRowRefusal(invoke, m_radioModel)")}},
        // iPhone app plan Task 34 (R-IOS-13): every transmit refusal's
        // sentence. The device name put into two of them is the operator's
        // own word (ruling 4.3); a band plan reason is the band plan's
        // sentence, passed on only when it is plain.
        // Level Cal: the calibration run's refusals and messages, sent as
        // startLevelCalibration's result and as radio's levelCalMessage.
        {"src/core/LevelCalibrationRun.cpp", {}, {}, 8, {},
         {// A search's failure, one of this file's messages.
          QStringLiteral("m_failure")}},
        {"src/core/LevelCalibrationService.cpp", {},
         {// AppSettings keys, never shown.
          "RX1_", "DspOptionsBufferSizePhoneRx"},
         2, {},
         {QStringLiteral("m_run->start(levelDbm, frequencyHz)"), QStringLiteral("refused")}},
        {"src/core/safety/TxRefusal.cpp", {}, {}, 15,
         {QStringLiteral("holderName"), QStringLiteral("holderShortName")},
         {QStringLiteral("reason"), QStringLiteral("std::move(text)"),
          QStringLiteral("bandPlanReasonIsPlain(reason) ? reason : QStringLiteral(\"The band plan does not allow transmitting here.\")")}},
        // iPhone app plan Task 34: MoxController records and reports a
        // refusal it is given (the band plan's, the interlock's, the
        // keying gate's), whose words are scanned where they are written.
        {"src/core/MoxController.cpp", {QStringLiteral("reportRefusal")}, {}, 0, {},
         {QStringLiteral("reason"), QStringLiteral("refusal")}},
        {"src/core/MoxController.h", {QStringLiteral("lastRefusal")}, {}, 0, {}, {}},
        // iPhone app Task 13 (R-IOS-08): the device administration verbs'
        // command.result, forwarded by the dispatcher as result.reason; the
        // log lines never reach an app.
        {"src/core/session/StationDevicesFacade.cpp", {},
         {"Could not save", "A paired device was removed", "The pairing token was retired"},
         8},
        // The label rule a refused station.rename gives.
        {"src/core/security/StationLabel.cpp", {QStringLiteral("ruleText")}, {}, 1},
        // property.result for a write the mirror refuses.
        {"src/core/session/StateMirror.cpp", {}, {}, 5, {},
         {// A function of this file, and a model's applyMirroredValue
          // (the hook), both scanned.
          QStringLiteral("writableButOutboundReason( MirrorSchema::shortClassName(className), "
                         "prop.name)"),
          QStringLiteral("hookReason")}},
        // PureSignal's command.result and its lastActionError.
        {"src/core/session/PureSignalSessionFacade.cpp", {}, {}, 15, {},
         {// finishOperation's parameter and the store's import error, both
          // from this file; lastActionError as a remote window receives it.
          QStringLiteral("reason"), QStringLiteral("imported.error"),
          QStringLiteral("value.toString()"),
          // R-R3-49 (parity Task 7): armingRefusal(), this file's own
          // literal or RadioModel::onAirReason (scanned with StationServer).
          QStringLiteral("isArmingAction(action) ? armingRefusal() : QString()")}},
        // Model and correction files; the store's and validator's own
        // messages are detail for the log unless isOperatorMessage says
        // otherwise (DspAssetService::rejectDetail).
        // The operator's own label for a model, and "the bundled small (large)
        // model" (bundledNr3Name).
        {"src/core/dsp/DspAssetService.cpp", {}, {}, 30,
         {QStringLiteral("label"), QStringLiteral("bundledNr3Name(id)")},
         {// rejectDetail's operator message (isOperatorMessage, scanned) or
          // its plain fallback, and the statuses this file words.
          QStringLiteral("detail"), QStringLiteral("plain"), QStringLiteral("error"),
          QStringLiteral("problems.join(QLatin1Char(' '))"), QStringLiteral("problem"),
          QStringLiteral("none")}},
        {"src/core/dsp/DspAssetValidation.cpp", {QStringLiteral("isOperatorMessage")}, {}, 6},
        // The explanation it restores is one this file words.
        {"src/core/dsp/NnrAdapter.cpp", {}, {}, 6, {}, {QStringLiteral("before.explanation")}},
        // R-R3-39: with a receive lane, RxChannel refuses at once with the
        // same words NnrAdapter uses (the lane's own refusal is NnrAdapter's).
        {"src/core/RxChannel.cpp",
         {QStringLiteral("setNnrTuning"), QStringLiteral("setNnrDiagnostics"),
          QStringLiteral("selectNr")},
         {}, 4},
        // The model paths' refusal reaches nnr.applyModelSelection.
        {"src/core/WdspEngine.cpp", {QStringLiteral("setNnrModelPaths")}, {}, 2},
        // A refused PureSignal settings write (property.result).
        {"src/models/PureSignalSettings.cpp",
         {QStringLiteral("isValid"), QStringLiteral("apply")}, {}, 5, {},
         // The reject lambda's parameter: the literals isValid passes it.
         {QStringLiteral("message")}},
        // The link version refusal and the takeover, sent in session.end.
        // Version numbers, "Update the Core." or "Update this app.", and
        // the other app's network address (WebSocketTransport::peerDescription).
        // The operator's ruling of 2026-09-26: the session.end every app
        // gets when the Core restarts its run on another radio; the radio's
        // name as it reports itself. The chooser's refusal when the run
        // stops before the change could run (radioChangeStoppedReason).
        {"src/core/daemon/DaemonApp.cpp",
         {QStringLiteral("radioChangeReason"), QStringLiteral("radioChangeStoppedReason"),
          QStringLiteral("tryCompleteStationRelease")},
         {}, 3, {QStringLiteral("radioName")}},
        {"src/core/session/SessionEndReasons.cpp",
         {QStringLiteral("takenOver"), QStringLiteral("versionRefused")}, {}, 4,
         {QStringLiteral("coreNewest"), QStringLiteral("appNewest"), QStringLiteral("update"),
          QStringLiteral("otherApp")}},
        // The window's reason it keeps: its own parameter, from this file.
        // Parity Task 12: and its transmit edit reasons (txKept, bypassKept),
        // from MainWindow's StationClient reasons, shown through
        // OperatorReasonText.
        {"src/core/accessories/AlexAntennaFacade.cpp", {}, {}, 6, {},
         {QStringLiteral("kept"), QStringLiteral("txKept"), QStringLiteral("bypassKept")}},
        // The attenuator's range in dB.
        {"src/core/StepAttenuatorFacade.cpp", {}, {}, 6,
         {QStringLiteral("lo"), QStringLiteral("hi"),
          // R-R3-49 (parity Task 5): the ATT on TX value's range in dB.
          QStringLiteral("m_values.minDb"), QStringLiteral("kMaxAttOnTxDb")},
         {QStringLiteral("kept")}},
        {"src/core/IoBoardHl2Facade.cpp", {}, {}, 1},
        // Task 16 (receiver and transmit gaps plan): receive only's reason,
        // what a refused key and a disabled transmit button show.
        // Task 16 fix wave (M2): the words setMox refuses with, which the
        // TGXL autotune and the Tuner applet's TUNE show too.
        // Merge of the trunk into the transmit lane: the same gate as a
        // TxRefusal (transmitBlockRefusal), its words TxRefusal.cpp's or
        // the reason setRxOnly was given.
        {"src/core/MoxController.cpp",
         {QStringLiteral("defaultRxOnlyReason"), QStringLiteral("transmitBlockReason"),
          QStringLiteral("transmitBlockRefusal"),
          // Fix wave 2: the interlock's refusal, TxRefusal.cpp's words.
          QStringLiteral("interlockRefusal"),
          // iPhone app plan Task 77: the keying gate's refusal of a
          // program's key, TxRefusal.cpp's words.
          QStringLiteral("programKeyRefusal")}, {}, 3, {},
         // The reason setRxOnly was given (RadioModel::rxOnlyReason, scanned),
         // and TxRefusal.cpp's refusals (scanned there). R-R3-46 (HL2 port
         // part 2): the reason setTxInhibited was given
         // (RadioModel::ioBoardFaultReason, scanned).
         {QStringLiteral("m_rxOnlyReason"), QStringLiteral("TxRefusals::stationReceiveOnly()"),
          QStringLiteral("TxRefusals::txInhibited()"), QStringLiteral("m_txInhibitReason"),
          // TX safety (2026-09-30): the lost radio link's refusal.
          QStringLiteral("TxRefusals::radioLinkDown()"),
          QStringLiteral("TxRefusals::radioLinkDown().text")}},
        {"src/core/MoxController.h",
         {QStringLiteral("rxOnlyReason"), QStringLiteral("txInhibitReason")}, {}, 0, {},
         // The reason setRxOnly was given (RadioModel::rxOnlyReason, scanned),
         // and the one setTxInhibited was given (RadioModel::ioBoardFaultReason,
         // scanned).
         {QStringLiteral("m_rxOnlyReason"), QStringLiteral("m_txInhibitReason")}},
        {"src/models/RadioModel.cpp",
         {QStringLiteral("rxOnlyForcedReason"), QStringLiteral("rxOnlyReason"),
          // TX safety (2026-09-30): the lost radio link's lock.
          QStringLiteral("radioLinkDownReason"),
          // iPhone app plan Task 77: a device's Tuner Genius autotune.
          QStringLiteral("startTgxlAutotuneFor"),
          // Task 77 fix round 2: the cycle's own refusals, which
          // startTgxlAutotuneFor passes on to the device.
          QStringLiteral("beginTgxlAutotune")}, {}, 1, {},
         // Both scanned here and in MoxController.cpp.
         {QStringLiteral("m_rxOnlyForced ? rxOnlyForcedReason() : "
                         "MoxController::defaultRxOnlyReason()"),
          // Task 77: the transmit block's words (MoxController's
          // transmitBlockReason, scanned there).
          QStringLiteral("transmitBlockReasonAlongside(QString())"),
          // startTgxlAutotuneFor's refusal: its own literals (scanned
          // here) or the transmit block's words above.
          QStringLiteral("refusal"),
          // Task 77 fix wave, I3: the on-air refusal's words
          // (TxRefusal.cpp, scanned there).
          QStringLiteral("TxRefusals::radioOnAir().text"),
          // TX safety (2026-09-30): the lost radio link's words
          // (TxRefusal.cpp, scanned there).
          QStringLiteral("TxRefusals::radioLinkDown().text"),
          // Task 77 fix round 2: beginTgxlAutotune's refusal (its own
          // literals and the words above, all scanned here), passed on by
          // startTgxlAutotuneFor.
          QStringLiteral("notStarted"), QStringLiteral("busy"),
          // beginTgxlAutotune's transmit block, with its own literal
          // alongside (both scanned: MoxController.cpp and here).
          QStringLiteral("transmitBlockReasonAlongside(remoteReason)"),
          // Carrier availability: tunerTuneEndedReason's six literal
          // reasons are scanned in the RadioModel.cpp entry below.
          QStringLiteral("tunerTuneEndedReason(TunerTuneEnd::CarrierNotStarted)")}},
        // Tune-ended lane: the words a device's Tuner Genius autotune that
        // ended before its carrier keyed is told (notice tuneEnded).
        {"src/models/RadioModel.cpp", {QStringLiteral("tunerTuneEndedReason")}, {}, 6},
        // A receiver count, and a frequency in MHz.
        {"src/core/SliceStreamAllocator.cpp", {}, {}, 4,
         {QStringLiteral("count"),
          QStringLiteral("QString::number(frequencyHz / 1.0e6, 'f', 4)")}},
        // Power in watts.
        {"src/core/StationAccessoryData.cpp", {}, {}, 4,
         {QStringLiteral("forwardW"), QStringLiteral("limitW")}},
        // The port number and the Core's addresses (blockedReason).
        {"src/core/StationTciController.cpp", {}, {}, 1,
         {QStringLiteral("port"), QStringLiteral("stationAddresses.join(QStringLiteral(\", \"))"),
          // A setting's range (StationTciModel::settingsTable()).
          QStringLiteral("setting->min"), QStringLiteral("setting->max")},
         {// TciServer's own error text, kept for the Core's log line only.
          QStringLiteral("error"),
          // The unknown-client sentence is scanned in this file.
          QStringLiteral("unknownClientReason()")}},
        // The SWR limit's range, one decimal.
        {"src/core/settings/SettingsProxyServer.cpp", {}, {}, 3,
         {QStringLiteral("kSwrProtectionLimitMin, 0, 'f', 1"),
          QStringLiteral("kSwrProtectionLimitMax, 0, 'f', 1")},
         {// Functions of this file and SettingsScope's refusal, scanned.
          QStringLiteral("otherRadioRefusal(key)"), QStringLiteral("refusal"),
          QStringLiteral("modelOwnedSettingsRefusal(key)")}},
        {"src/core/settings/SettingsScope.cpp", {QStringLiteral("modelOwnedSettingsRefusal")},
         {}, 5},
        // iPhone app plan Task 23: the audio context's opusBitrateRefusal.
        {"src/core/session/media/RemoteAudioContext.cpp",
         {QStringLiteral("opusBitrateNotOfferedReason")}, {}, 1},
        // The display refusals and retirements (rejected, allocation-result).
        {"src/core/session/media/DaemonMediaController.cpp", {},
         // statsSummary(): a log line's text; unkeyEventLines(): the unkey
         // event lines, log text (unkeyEventLogText).
         QStringList{QStringLiteral("largestKeyframe=")} + unkeyEventLogText(),
         19, {},
         {// Parameters and fields that carry this file's own reasons.
          QStringLiteral("reason"), QStringLiteral("prior.reason"),
          QStringLiteral("admitted.refusal"), QStringLiteral("stream.profileRefusal"),
          QStringLiteral("m_headphones.profileRefusal"), QStringLiteral("m_audioProfileRefusal"),
          // iPhone app plan Task 23: a refused opusBitrate's words, scanned
          // in RemoteAudioContext.cpp's entry.
          QStringLiteral("opusBitrateNotOfferedReason()"),
          QStringLiteral("m_audioBitrateRefusal"),
          // Codes, not reasons (section 17): receiver audio off and
          // profile refusal codes, spectrum limit reasons, retire reasons.
          QStringLiteral("headphonesBlockedBy().value_or(RemoteAudioOffReason::RadioOffline)"),
          QStringLiteral("grantReason(grant)"), QStringLiteral("grantReason(reasonGrant)"),
          QStringLiteral("context.offReason"),
          QStringLiteral("RemoteAudioProfileRefusal::NotAllowed"),
          QStringLiteral("RemoteAudioProfileRefusal::Unavailable"),
          QStringLiteral("RemoteAudioOffReason::EncoderUnavailable"),
          QStringLiteral("QString::fromLatin1(kRetireReasonSourceRetune)"),
          QStringLiteral("QString::fromLatin1(kRetireReasonStreamBindingChanged)"),
          QStringLiteral("QString::fromLatin1(kRetireReasonSliceRemoved)"),
          // Fix wave 2: the budget refusal, shared with the client,
          // DisplayBudget.h (scanned below).
          QStringLiteral("QString::fromLatin1(kDisplayBudgetRefusalReason)"),
          // The Core's media-peer drop reasons, shared with the client,
          // MediaPeer.h (scanned below).
          QStringLiteral("QString::fromLatin1(m_peerLost ? kMediaPeerLostReason : "
                         "kMediaPeerClosedReason)")}},
        {"src/core/session/media/SpectrumEndpoint.h", {}, {}, 3},
        // Fix wave 2: kDisplayBudgetRefusalReason.
        {"src/core/session/media/DisplayBudget.h", {}, {}, 1},
        // kMediaPeerLostReason and kMediaPeerClosedReason.
        {"src/core/session/media/MediaPeer.h", {}, {}, 2},
        {"src/models/AccessoryDataModel.cpp", {QStringLiteral("readOnlyReason")}, {}, 1},
        {"src/models/AccessorySettingsModel.cpp", {QStringLiteral("readOnlyReason")}, {}, 1},
        // A Power Genius or Tuner Genius on another network: its
        // connectionError, through the controllers' rejectIdentity. The
        // device's name and its address.
        {"src/core/StationNetwork.cpp", {QStringLiteral("offNetworkReason")}, {}, 1,
         {QStringLiteral("deviceName, address")}},
        // Reset amp error's refusal (resetRfKitError). R-R3-49 (parity Task
        // 10): OPERATE, antenna and TCI mode, through ampAdmitted.
        {"src/core/StationRfKitController.cpp",
         {QStringLiteral("ampAdmitted"), QStringLiteral("resetError"),
          QStringLiteral("setOperate"), QStringLiteral("setAntenna"),
          QStringLiteral("setTciMode")}, {}, 2},
        // The amp's and tuner's own settings: the refusals of their
        // commands and the answers the `accessorySettings` object carries.
        // The device's name ("Power Genius", "Tuner Genius").
        {"src/core/StationDeviceSettings.cpp", {}, {}, 10,
         {QStringLiteral("deviceName()"), QStringLiteral("device")},
         {// validateNetwork's result, this file's own literals.
          QStringLiteral("problem")}},
        {"src/models/AmplifierModel.cpp",
         {QStringLiteral("readOnlyReason"), QStringLiteral("receiveOnlyOperateReason")}, {}, 2},
        {"src/models/RfKitModel.cpp", {QStringLiteral("readOnlyReason")}, {}, 1},
        {"src/models/StationTciModel.cpp", {QStringLiteral("readOnlyReason")}, {}, 1},
        // Parity Task 19 (R-IOS-25): the spots.* refusals and the
        // `spotSources` object's read-only reason.
        // Parity Task 21 (R-IOS-18): the station radio verbs' refusals and
        // the Core's waiting lines.
        {"src/core/station/StationRadios.cpp",
         {QStringLiteral("choose"), QStringLiteral("unknownRadioReason"),
          QStringLiteral("switchingReason"), QStringLiteral("inUseReason"),
          QStringLiteral("pairedDeviceReason"),
          QStringLiteral("select"), QStringLiteral("setModel"), QStringLiteral("forget")},
         {}, 6, {},
         {// The refuse helper's parameter and the three reasons above.
          QStringLiteral("why"), QStringLiteral("inUseReason()"),
          QStringLiteral("switchingReason()"), QStringLiteral("unknownRadioReason()")}},
        // iPhone plan Task 22 (R-IOS-26): and the freedv.* refusals and
        // FreeDV Reporter's start (the reason it did not register).
        {"src/core/SpotSourceHost.cpp",
         {QStringLiteral("readOnlyReason"), QStringLiteral("connectSource"),
          QStringLiteral("disconnectSource"), QStringLiteral("sendCommand"),
          QStringLiteral("setFreedvMessage"), QStringLiteral("sendFreedvQsy"),
          QStringLiteral("startFreedvWith"), QStringLiteral("setFreedvHidden")},
         {}, 16, {QStringLiteral("wanted")},
         {// The refuse helper's parameter: this entry's own literals.
          QStringLiteral("why")}},
        {"src/models/SliceModel.cpp",
         {QStringLiteral("activeWriteReason"), QStringLiteral("applyMirroredValue")}, {}, 5},
        // Slice control plan Task 5: a remote window's copy of the Core's
        // listener words (StationServer::listenerChangeReason), shown when
        // it holds back a change to a slice it only listens to. The slice
        // letter (A to P) and the controller's name, the operator's own
        // word (ruling 4.3).
        {"src/core/session/SliceAccessMirror.cpp", {QStringLiteral("listenerReason")}, {}, 3,
         {QStringLiteral("letter"), QStringLiteral("owner"),
          // Slice control plan Task 17: the controller's kind word.
          QStringLiteral("kind")}},
        {"src/models/TunerModel.cpp", {QStringLiteral("applyMirroredValue")}, {}, 2},
        {"src/models/RadioModel.cpp",
         {QStringLiteral("applyMirroredValue"), QStringLiteral("setFourO3AEnabledForStation"),
          QStringLiteral("setStationTciForStation"),
          QStringLiteral("setStationTciOptionsForStation"),
          QStringLiteral("setStationTciSettingsForStation"),
          QStringLiteral("disconnectStationTciClientForStation"),
          QStringLiteral("setTxInterlockPolicyForStation"),
          QStringLiteral("setPgxlPowerCapForStation"),
          QStringLiteral("clearAccessoryFaultsForStation"),
          QStringLiteral("configureTgxlForStation"), QStringLiteral("disconnectTgxlForStation"),
          QStringLiteral("configurePgxlForStation"), QStringLiteral("disconnectPgxlForStation"),
          QStringLiteral("setPgxlConnectionSettingsForStation"),
          QStringLiteral("setRfKitEnabledForStation"), QStringLiteral("configureRfKitForStation"),
          QStringLiteral("disconnectRfKitForStation"), QStringLiteral("resetRfKitErrorForStation"),
          QStringLiteral("refuseNoStationDevice"), QStringLiteral("setPgxlNameForStation"),
          QStringLiteral("setPgxlHardwareForStation"), QStringLiteral("setPgxlNetworkForStation"),
          QStringLiteral("savePgxlSettingsForStation"),
          QStringLiteral("readPgxlSettingsForStation"), QStringLiteral("setTgxlNameForStation"),
          QStringLiteral("setTgxlNetworkForStation"),
          QStringLiteral("saveTgxlSettingsForStation"),
          QStringLiteral("readTgxlSettingsForStation"),
          // R-R3-49: the Tuner Genius's antenna, operate and bypass.
          QStringLiteral("stationTgxlControlAllowed"),
          // R-R3-49 (parity Task 1): the Core's one on-the-air refusal.
          QStringLiteral("onAirReason"), QStringLiteral("stationOnAirRefusal"),
          // iPhone app plan Task 77 fix round 3: the Power Genius also
          // waits while the Tuner Genius tunes.
          QStringLiteral("tunerTuningReason"), QStringLiteral("pgxlSwitchRefusal"),
          // Round 4: and while the amplifier is still switching.
          QStringLiteral("ampStillSwitchingReason"),
          QStringLiteral("setTgxlAntennaForStation"), QStringLiteral("setTgxlOperateForStation"),
          QStringLiteral("setTgxlBypassForStation"),
          // R-R3-49 (parity Task 8): the relay nudge, the Core's LAN scan and
          // the saved address.
          QStringLiteral("moveTgxlRelayForStation"), QStringLiteral("scanTgxlLanForStation"),
          QStringLiteral("setTgxlAddressForStation"),
          // R-R3-49 (parity Task 9): the Power Genius's OPERATE and STANDBY,
          // the Core's LAN scan for it and the saved address.
          QStringLiteral("stationPgxlControlAllowed"), QStringLiteral("setPgxlOperateForStation"),
          QStringLiteral("scanPgxlLanForStation"), QStringLiteral("setPgxlAddressForStation"),
          // R-R3-49 (parity Task 10): the RF-Kit's OPERATE and STANDBY,
          // antenna, TCI mode and saved address.
          QStringLiteral("stationRfKitControlAllowed"),
          QStringLiteral("setRfKitOperateForStation"),
          QStringLiteral("setRfKitAntennaForStation"),
          QStringLiteral("setRfKitTciModeForStation"),
          QStringLiteral("setRfKitAddressForStation"),
          // R-R3-49 (parity Task 2): the Tune Power slider's command.
          QStringLiteral("setTunePowerForTxBandForStation"),
          // R-R3-49 (parity Task 3): the TX profile commands and the RADE
          // vocoder reset.
          QStringLiteral("selectTxProfileForStation"),
          QStringLiteral("saveTxProfileForStation"),
          QStringLiteral("deleteTxProfileForStation"),
          QStringLiteral("resetRadeVocoderForStation"),
          // R-R3-49 / R-IOS-18: the paProfile verbs.
          QStringLiteral("paProfileActionForStation"),
          QStringLiteral("setNnrDiagnosticMode"),
          QStringLiteral("applyNnrModelSelection"), QStringLiteral("addNotchFromStation"),
          QStringLiteral("moveNotchFromStation"), QStringLiteral("setNotchActiveFromStation"),
          QStringLiteral("deleteNotchFromStation"), QStringLiteral("requestIoBoardProbe"),
          // R-IOS-27, R-IOS-06: notch.addAtSlice, which relays
          // addNotchFromStation's reasons.
          QStringLiteral("addTnfFromStation"),
          QStringLiteral("nr3CannotRunReason"),
          // R-R3-49: DFNR's, sent as dspAssets' dfnrModelStatus.
          QStringLiteral("dfnrCannotRunReason"),
          // R-R3-49 (tx-followup-3): MNR's, sent as dspAssets' mnrStatus,
          // BNR's, and the one reason the flag, the menu and the refusals
          // read.
          QStringLiteral("mnrCannotRunReason"), QStringLiteral("bnrCannotRunReason"),
          QStringLiteral("nrCannotRunReason"), QStringLiteral("nrCannotRunInThisBuildReason"),
          // The slice cap reason, relayed by the addSlice and addSliceOnPan
          // verbs' results.
          QStringLiteral("sliceCapReason"),
          // iPhone app Task 73: why a device's saved slice did not fit (the
          // Core's log today; Task 74's slicesNotRestored notice).
          QStringLiteral("restoreSliceFor"),
          // R-IOS-27, R-IOS-06: the desktop's band button, whose refusal
          // (bandClickIgnored) slice.selectBand relays.
          QStringLiteral("onBandButtonClicked"),
          // R-R3-46 (parity Task 14): HL2 Options' I2C tool and Pin
          // Control, whose refusals requestIoBoardI2c and setIoBoardOutput
          // relay.
          QStringLiteral("requestIoBoardI2c"), QStringLiteral("setIoBoardOutput"),
          QStringLiteral("ioBoardNoAnswerReason"),
          QStringLiteral("ioBoardI2cUnreachableReason"),
          // R-R3-49 (parity Task 16): dsp.filterResponse's refusals.
          QStringLiteral("filterResponseForStation"),
          // R-R3-46 (HL2 port part 2, txInhibitReasonVersion 1): the I/O
          // board fault's reason, which txInhibitReason sends and the
          // desktop's transmit block shows.
          QStringLiteral("ioBoardFaultReason"), QStringLiteral("refreshTxInhibitReason"),
          QStringLiteral("txInhibitReason"),
          // R-R3-49 / R-IOS-27 (Setup description version 20): the PA
          // on-air gate's refusals, for the paProfile verbs and a window's
          // raw PA keys.
          QStringLiteral("paOnAirLockedReason"), QStringLiteral("paHolderOnlyReason"),
          QStringLiteral("paOnAirEditRefusal"), QStringLiteral("paValueRangeRefusal"),
          QStringLiteral("paRowRangeRefusal"), QStringLiteral("paSettingOnAirRefusal"),
          // Setup description version 22: the RX buffer sizes' on-air
          // lock (paOnAirLockedReason's words).
          QStringLiteral("dspBufferOnAirLockedReason")},
         {// This app's own branch in a remote window (role Remote), shown
          // through OperatorReasonText; never sent by the Core.
          "There is no station session."},
         20,
         // sliceCapReason: the radio's product label and the slice count,
         // worded "1 slice" or "N slices" from its own literals. R-R3-49
         // (parity Task 3): name, the transmit profile's own name.
         {QStringLiteral("radioLabel, slices"), QStringLiteral("slices"), QStringLiteral("cap"),
          QStringLiteral("name"),
          // R-R3-49 / R-IOS-18: a PA profile's own name.
          QStringLiteral("request.name"),
          // onBandButtonClicked: the band's own label ("40m").
          QStringLiteral("bandLabel(band)"),
          // ioBoardFaultReason: the board's fault code, a number.
          QStringLiteral("code")},
         {// The refuse lambdas' parameter (literals of these functions),
          // the facades' and allocators' results (scanned), and the notch
          // refusals, constants of this file checked in
          // notchConstantsArePlain below.
          QStringLiteral("text"), QStringLiteral("result.reason"),
          // R-R3-49 / R-IOS-18: paProfileActionForStation's name check, its
          // literals in that function.
          QStringLiteral("nameRefusal(name)"),
          QStringLiteral("outcome.reason"), QStringLiteral("kUnknownNotchReason"),
          QStringLiteral("kNotchListBusyReason"),
          // R-R3-49 (parity Task 1): onAirReason, a function of this file
          // scanned here.
          QStringLiteral("onAirReason()"),
          // Task 77 fix round 3: tunerTuningReason, likewise.
          QStringLiteral("tunerTuningReason()"),
          // Round 4: ampStillSwitchingReason, likewise.
          QStringLiteral("ampStillSwitchingReason()"),
          // R-R3-49 (parity Task 2): TransmitModel::settingRangeRefusal,
          // scanned below.
          QStringLiteral("range"),
          // R-R3-46 (parity Task 14): ioBoardI2cUnreachableReason and
          // ioBoardNoAnswerReason, functions of this file scanned here;
          // the link's own reason for a request it could not send; and
          // stationOnAirRefusal's reason (onAirReason, scanned here).
          QStringLiteral("unreachable"), QStringLiteral("ioBoardNoAnswerReason()"),
          QStringLiteral("onAir"),
          // R-R3-46 (HL2 port part 2): ioBoardFaultReason, a function of
          // this file scanned here, held in m_txInhibitReason; and in a
          // remote window, the Core's txInhibitReason (that same function's
          // words) mirrored as it is sent.
          QStringLiteral("m_txInhibit.inhibited() && m_txInhibit.lastSource() == "
                         "safety::TxInhibitMonitor::Source::IoBoardFault ? "
                         "ioBoardFaultReason(m_txInhibit.ioBoardFaultCode()) : QString()"),
          QStringLiteral("reason"), QStringLiteral("m_txInhibitReason"),
          QStringLiteral("value.toString()"),
          // R-R3-49 / R-IOS-27: the PA on-air gate. paOnAirLockedReason,
          // paHolderOnlyReason, paOnAirEditRefusal, paValueRangeRefusal
          // and paRowRangeRefusal are functions of this file scanned here;
          // each expression below passes one of their results on.
          QStringLiteral("paOnAirEditRefusal(profileAction, request.band, "
                         "request.requesterHoldsTransmit)"),
          QStringLiteral("paOnAirEditRefusal(false, txBand, requesterHoldsTransmit)"),
          QStringLiteral("refusal"),
          QStringLiteral("paValueRangeRefusal(request.action, request.value)"),
          QStringLiteral("rangeRefusal"),
          // Slice control plan Task 5: a held NNR diagnostics request says
          // the words SliceAccessMirror::listenerReason made (scanned).
          QStringLiteral("slice->readOnlyListenerReason()")}},
        // iPhone app plan Task 25 (R-IOS-18): a `vax` level outside 0 to 1,
        // in property.result.
        {"src/core/session/StationVaxFacade.cpp", {QStringLiteral("levelRefusal")}, {}, 1},
        // R-R3-49 (parity Task 2): a transmit setting's range, in
        // property.result and setTunePowerForTxBand's command.result. The
        // inserts are the setters' own range numbers.
        // R-IOS-13 / R-R3-49 (txEqCurveVersion 2): txEq.setCurve's refusals.
        {"src/core/ParaEqCurve.cpp", {QStringLiteral("txEqPointsFromCurveJson")}, {}, 7, {},
         // refuse(why): each literal handed to it is scanned here.
         {QStringLiteral("why")}},
        // R-R3-49 (transmitSettingsVersion 15): cfc.setProfile's refusals.
        {"src/core/CfcProfile.cpp", {QStringLiteral("fromPublishedJson")}, {}, 10, {},
         // refuse(why) and the shared notUnderstood: each literal is scanned here.
         {QStringLiteral("why"), QStringLiteral("notUnderstood")}},
        {"src/models/TransmitModel.cpp", {QStringLiteral("settingRangeRefusal")}, {}, 7,
         {QStringLiteral("hi"), QStringLiteral("kVoxThresholdDbMin"),
          QStringLiteral("kVoxThresholdDbMax"), QStringLiteral("kVoxHangTimeMsMin"),
          QStringLiteral("kVoxHangTimeMsMax"), QStringLiteral("kCpdrLevelDbMin"),
          QStringLiteral("kCpdrLevelDbMax"), QStringLiteral("kAmCarrierLevelMin"),
          QStringLiteral("kAmCarrierLevelMax"), QStringLiteral("kMicGainDbMin"),
          QStringLiteral("kMicGainDbMax"),
          // R-R3-49 (parity Task 4): the TX EQ, CFC, phase rotator, leveler
          // and ALC ranges, and the ten-value band arrays' ranges.
          QStringLiteral("kTxEqPreampDbMin"), QStringLiteral("kTxEqPreampDbMax"),
          QStringLiteral("kTxEqNcMin"), QStringLiteral("kTxEqNcMax"),
          QStringLiteral("kCfcPrecompDbMin"), QStringLiteral("kCfcPrecompDbMax"),
          QStringLiteral("kCfcPostEqGainDbMin"), QStringLiteral("kCfcPostEqGainDbMax"),
          QStringLiteral("kPhaseRotatorFreqHzMin"), QStringLiteral("kPhaseRotatorFreqHzMax"),
          QStringLiteral("kPhaseRotatorStagesMin"), QStringLiteral("kPhaseRotatorStagesMax"),
          QStringLiteral("kTxLevelerMaxGainDbMin"), QStringLiteral("kTxLevelerMaxGainDbMax"),
          QStringLiteral("kTxLevelerDecayMsMin"), QStringLiteral("kTxLevelerDecayMsMax"),
          QStringLiteral("kTxAlcMaxGainDbMin"), QStringLiteral("kTxAlcMaxGainDbMax"),
          QStringLiteral("kTxAlcDecayMsMin"), QStringLiteral("kTxAlcDecayMsMax"),
          QStringLiteral("kTxEqBandDbMin"), QStringLiteral("kTxEqBandDbMax"),
          QStringLiteral("kTxEqFreqHzMin"), QStringLiteral("kTxEqFreqHzMax"),
          QStringLiteral("kCfcCompressionDbMin"), QStringLiteral("kCfcCompressionDbMax"),
          QStringLiteral("kCfcEqFreqHzMin"), QStringLiteral("kCfcEqFreqHzMax"),
          QStringLiteral("kCfcPostEqBandGainDbMin"), QStringLiteral("kCfcPostEqBandGainDbMax"),
          // R-R3-49 (parity Task 5): the anti-VOX gain and two-tone ranges.
          QStringLiteral("kAntiVoxGainDbMin"), QStringLiteral("kAntiVoxGainDbMax"),
          QStringLiteral("kTwoToneFreq1HzMin"), QStringLiteral("kTwoToneFreq1HzMax"),
          QStringLiteral("kTwoToneFreq2HzMin"), QStringLiteral("kTwoToneFreq2HzMax"),
          QStringLiteral("kTwoTonePowerMin"), QStringLiteral("kTwoTonePowerMax"),
          QStringLiteral("kTwoToneFreq2DelayMsMin"), QStringLiteral("kTwoToneFreq2DelayMsMax")}},
        // iPhone app plan Task 39: txState's stopReason is the wire code
        // (linkLost, timeOut, ...), never shown; its words are stopText,
        // scanned with the property texts. Task 38's lastTransmitStopReason
        // is a code, which half and a limit, never shown either.
        {"src/core/session/TransmitStateFacade.h", {QStringLiteral("stopReason")}, {}, 0, {},
         {QStringLiteral("m_stopReason")}},
        {"src/models/RadioModel.h", {QStringLiteral("lastTransmitStopReason")}, {}, 0, {},
         {QStringLiteral("m_lastTransmitStopReason")}},
    };
    return sources;
}

// Reason sites that never send to an app, with why.
struct AppSideReason {
    const char* file;
    const char* function;  // Empty: every match in the file.
    const char* why;
};

const QList<AppSideReason>& appSideReasons()
{
    static const QList<AppSideReason> sites{
        {"src/core/session/RemoteTransmitClient.h", "micSourceReason",
         "this window's pending/refused source getter; Core sentences scanned at their source"},
        {"src/models/RadioModel.cpp", "micSourceChangeReason",
         "remote-only window preflight; shared radio-mic sentences scanned in RemoteMicSource.h"},
        {"src/core/session/StationClient.cpp", "",
         "the app's end of the link: its own reasons are shown through OperatorReasonText"},
        {"src/core/session/StationClient.h", "", "the app's end of the link"},
        // iPhone app Task 18: the desktop's side of pairing. Its own reasons
        // are shown through OperatorReasonText; the one pair.fail it sends
        // only tells the Core the code did not match, and the operator reads
        // the Core's answer.
        {"src/core/session/StationPairingClient.cpp", "",
         "the app's end of pairing: its own reasons are shown through OperatorReasonText"},
        {"src/core/session/StationPairingClient.h", "", "the app's end of pairing"},
        // iPhone app plan Task 28 fix wave (R-IOS-16): the saved Core's own
        // rule for connecting from anywhere, which the desktop shows as is.
        {"src/core/session/RemoteStationOptions.h", "serviceConnectRefusal",
         "a desktop's own reason for not offering to connect from anywhere"},
        {"src/core/session/SessionMessages.cpp", "", "encodes a reason it is given"},
        {"src/core/session/SessionMessages.h", "", "declares the messages"},
        {"src/core/session/SessionCommandDispatcher.h", "", "declares emitResult"},
        {"src/core/session/StationServer.h", "", "declares dropPeer"},
        {"src/core/session/media/DaemonMediaController.h", "", "declares sendRejected"},
        {"src/core/audio/RemoteVaxFeeder.cpp", "lastStopReason",
         "a remote window's VAX feeder: why its own audio stopped"},
        {"src/core/session/media/MediaPeer.cpp", "lastStartRefusal",
         "a code the window reads (StartRefusal), not text"},
        {"src/core/HardwareProfile.cpp", "profileForStation", "returns a hardware profile, not text"},
        {"src/core/StepAttenuatorFacade.h", "windowUnavailableReason",
         "a remote window's own reason the attenuator rows show"},
        {"src/core/accessories/AlexAntennaFacade.h", "windowUnavailableReason",
         "a remote window's own reason the antenna rows show"},
        {"src/core/accessories/AlexAntennaFacade.h", "txAntennasUnavailableReason",
         "a remote window's own reason the transmit antenna rows show (parity Task 12)"},
        {"src/core/accessories/AlexAntennaFacade.h", "rxBypassUnavailableReason",
         "a remote window's own reason RX bypass on TX shows (parity Task 12)"},
        {"src/core/TciServer.h", "operatorNoticeReason", "a remote window's own TCI notice"},
        {"src/models/RadioModel.cpp", "mirrorTxProfilesFromStation",
         "a window's TX profile requests: it shows the link's own reason for a request it "
         "could not send"},
        {"src/models/RadioModel.cpp", "noStationReason",
         "a remote window's own notice when it has no link to the Core"},
        // Fix round 1 (minor 4): the link-down words MOX, TUNE and 2-TONE
        // show, by state.
        {"src/models/RadioModel.cpp", "transmitLinkDownReason",
         "a window's own reason MOX, TUNE and 2-TONE are locked while a link is down"},
        // Level Cal 2: the hosting desktop's own Calibration tab shows it on
        // a disabled Start; no verb sends it. Its sentences are
        // StationServer's ownedElsewhereReason and listenerChangeReason
        // words (scanned there) with "another device" for the name.
        {"src/models/RadioModel.cpp", "levelCalHostSlice",
         "the hosting desktop's own reason its Level Cal Start is disabled"},
        // R-R3-49 (parity Task 16, trunk merge): why DFNR or MNR is
        // disabled on a Core too old to say, the window's own words.
        {"src/models/RadioModel.cpp", "noiseReductionNotSaidReason",
         "a remote window's own reason DFNR and MNR are disabled"},
        {"src/models/RadioModel.cpp", "coreFilterResponseUnavailableReason",
         "a remote window's own reason the high-resolution filter graph is disabled"},
        {"src/models/RadioModel.cpp", "reportStationAccessoryRefusal",
         "a remote window passes the Core's refusal on to its own pages"},
        // Addendum G-42 (scoped review): stores the Core's holder test,
        // whose words are TxRefusals::otherDeviceHolds (scanned there).
        {"src/models/RadioModel.h", "setOtherDeviceHoldsRefusal",
         "stores a probe the Core installs, not text"},
        {"src/models/RadioModel.h", "otherDeviceHoldsRefusal",
         "returns what that probe says"},
        // transmitSettingsVersion 11: why Disable HF PA is disabled on a
        // radio without the switch, the window's own words.
        {"src/models/RadioModel.cpp", "hfPaSwitchUnavailableReason",
         "a window's own reason Disable HF PA is disabled on this radio"},
        {"src/core/session/IStationLink.h", "pgxlDeviceSettingsUnavailableReason",
         "a remote window's own reason when its Core cannot take the request"},
        {"src/core/session/IStationLink.h", "tgxlDeviceSettingsUnavailableReason",
         "a remote window's own reason when its Core cannot take the request"},
        {"src/core/session/IStationLink.h", "tgxlControlUnavailableReason",
         "a remote window's own reason when its Core cannot take the request"},
        // R-R3-49 (parity Task 8): the relay nudge, LAN scan and address.
        {"src/core/session/IStationLink.h", "tgxlFullControlUnavailableReason",
         "a remote window's own reason when its Core cannot take the request"},
        // R-R3-49 (parity Task 9): the Power Genius's operate, scan and address.
        {"src/core/session/IStationLink.h", "pgxlFullControlUnavailableReason",
         "a remote window's own reason when its Core cannot take the request"},
        // R-R3-49 (parity Task 10): the RF-Kit's operate, antenna, TCI mode
        // and address.
        {"src/core/session/IStationLink.h", "rfKitFullControlUnavailableReason",
         "a remote window's own reason when its Core cannot take the request"},
        {"src/core/session/IStationLink.h", "transmitSettingsUnavailableReason",
         "a remote window's own reason when its Core cannot take the request"},
        // Addendum G-42: the window's own words when the Core gave none for
        // why this device may not change Extended transmit.
        {"src/core/session/IStationLink.h", "transmitPermissionReason",
         "a remote window's own fallback when its Core does not say why it may not transmit"},
        {"src/core/session/IStationLink.h", "filterPolicyUnavailableReason",
         "a remote window's own reason when its Core cannot take the request"},
        // R-R3-46 (parity Task 14): HL2 Options' I2C tool and Pin Control,
        // and the Alex tab's three transmit high-pass switches.
        {"src/core/session/IStationLink.h", "ioBoardI2cUnavailableReason",
         "a remote window's own reason when its Core cannot take the request"},
        {"src/core/session/IStationLink.h", "alexHpfSwitchesUnavailableReason",
         "a remote window's own reason when its Core cannot take the request"},
        // radioHardwareVersion 8: the Alex Filters tabs' receive filter rows.
        {"src/core/session/IStationLink.h", "alexHpfRowsUnavailableReason",
         "a remote window's own reason when its Core cannot take the request"},
        // radioHardwareVersion 11: HL2 Options' Enable CL2, CL2 frequency
        // and External 10 MHz.
        {"src/core/session/IStationLink.h", "hl2ClockUnavailableReason",
         "a remote window's own reason when its Core cannot take the request"},
        // radioHardwareVersion 13: HL2 Options' Swap audio channels.
        {"src/core/session/IStationLink.h", "hl2SwapAudioUnavailableReason",
         "a remote window's own reason when its Core cannot take the request"},
        // iPhone app plan Task 25: This Core's device list on a Core that
        // does not offer device administration to this window.
        {"src/core/session/IStationLink.h", "deviceAdminUnavailableReason",
         "a remote window's own reason when its Core cannot take the request"},
        {"src/core/session/IStationLink.h", "pairedDeviceAdminReason",
         "a remote window's own reason when its Core cannot take the request"},
        // Parity ruling C4: the rate box on a Core without setRadioSampleRate.
        {"src/core/session/IStationLink.h", "radioSampleRateUnavailableReason",
         "a remote window's own reason when its Core cannot take the request"},
        // Level Cal: Setup's level calibration Reset on a Core without
        // resetLevelCalibration.
        {"src/core/session/IStationLink.h", "levelCalibrationResetUnavailableReason",
         "a remote window's own reason when its Core cannot take the request"},
        // Level Cal: Setup's level calibration Start and Cancel on a Core
        // without startLevelCalibration.
        {"src/core/session/IStationLink.h", "levelCalibrationRunUnavailableReason",
         "a remote window's own reason when its Core cannot take the request"},
        // Level Cal fix wave: a slice on the other ADC's preamp choice on a
        // Core without stepAtt's rx2PreampMode.
        {"src/core/session/IStationLink.h", "rx2PreampModeUnavailableReason",
         "a remote window's own reason when its Core cannot take the request"},
        // R-R3-49 (parity Task 16): the filter graph's curve.
        {"src/core/session/IStationLink.h", "filterResponseUnavailableReason",
         "a remote window's own reason when its Core cannot take the request"},
        // R-IOS-18 (parity Task 21): the Core's radio.
        {"src/core/session/IStationLink.h", "stationRadiosUnavailableReason",
         "a remote window's own reason when its Core cannot take the request"},
        // iPhone app plan Task 25 (This Core page): the Core's paired devices.
        {"src/core/session/IStationLink.h", "deviceAdminUnavailableReason",
         "a remote window's own reason when its Core cannot take the request"},
        {"src/core/session/IStationLink.h", "pairedDeviceAdminReason",
         "a remote window's own reason when its Core cannot take the request"},
        {"src/core/session/IStationLink.h", "settingsHygieneUnavailableReason",
         "a remote window's own fallback when its Core cannot validate settings"},
        // G-38: Repair invalid settings on a Core before settingsHygieneVersion 2.
        {"src/core/session/IStationLink.h", "settingsRepairUnavailableReason",
         "a remote window's own reason when its Core cannot repair settings"},
        {"src/core/session/IStationLink.h", "modMonitorUnavailableReason",
         "a remote window's own fallback when its Core cannot send modulation readings"},
        // iPhone app plan Task 25 (This Core in a remote window): the
        // Core's paired devices.
        {"src/core/session/IStationLink.h", "deviceAdminUnavailableReason",
         "a remote window's own reason when its Core cannot manage devices"},
        {"src/core/session/IStationLink.h", "pairedDeviceAdminReason",
         "a remote window's own reason when it is not signed in as a paired device"},
        // R-IOS-26 / R-R3-49: 2 m as its own band.
        {"src/core/session/IStationLink.h", "band2mUnavailableReason",
         "a remote window's own reason when its Core does not have the 2 m band"},
        // Slice control plan Task 5.
        {"src/core/session/IStationLink.h", "sliceAccessUnavailableReason",
         "a remote window's own reason when its Core cannot share slices"},
        {"src/models/SliceModel.h", "readOnlyListenerReason",
         "getter of the words SliceAccessMirror::listenerReason made (scanned)"},
        {"src/core/SettingsHygiene.h", "remoteUnavailableReason",
         "getter for desktop state set by StationClient and read by diagnostics pages"},
        {"src/core/station/StationRadios.h", "waitingReason",
         "the Core's own line while it waits for a radio, kept with its list; no app is sent it"},
        // R-IOS-25 (parity Task 19): the Core's spot sources.
        {"src/core/session/IStationLink.h", "spotSourcesUnavailableReason",
         "a remote window's own reason when its Core cannot take the request"},
        // R-IOS-26 (iPhone plan Task 22): the Core's FreeDV Reporter.
        {"src/core/session/IStationLink.h", "stationFreedvUnavailableReason",
         "a remote window's own reason when its Core cannot take the request"},
        {"src/core/session/IStationLink.h", "stationTciServerUnavailableReason",
         "a remote window's own reason when its Core cannot manage its TCI apps"},
        {"src/core/SpotSourceHost.cpp", "reportStationRefusal",
         "a remote window shows the Core's refusal it was given"},
        // R-R3-49 (parity Task 22): the Core's log, logging and bundle. The
        // Core also answers a coreLog subscribe with it when it keeps no
        // log stream; tst_remote_core_log checks it is plain.
        {"src/core/session/IStationLink.h", "supportBundleUnavailableReason",
         "a remote window's own reason when its Core cannot take the request"},
        // radioHardwareVersion 10 (Alex LPF rows): the Alex-1 tab's
        // low-pass rows on an older Core, and the 6m/ByPass box on a
        // board without the bypass (both disabled with the reason shown).
        {"src/core/session/IStationLink.h", "alexLpfRowsUnavailableReason",
         "a remote window's own reason when its Core cannot take the request"},
        {"src/models/RadioModel.cpp", "lpfBypassUnavailableReason",
         "the reason shown on the disabled 6m/ByPass box, not a Core refusal"},
        // Setup description 24 (radio codec): why the Radio Mic (Orion-MkII)
        // rows are disabled on the Red Pitaya, shown on the disabled group.
        {"src/models/RadioModel.cpp", "orionMicPanelUnavailableReason",
         "the reason shown on the disabled Orion mic group, not a Core refusal"},
        {"src/models/RadioModel.cpp", "stationSupportUnavailableReason",
         "a remote window's own reason its Core-side support controls are disabled"},
        {"src/models/RadioModel.h", "rxFilter0Reason",
         "the filter badge's status label (AlexController), not a refusal"},
        {"src/models/RadioModel.h", "rxFilter1Reason",
         "the filter badge's status label (AlexController), not a refusal"},
        // Shared-input filters, ruling (d): why the receive low-pass is set
        // for another slice (lowPassHoldReason, scanned below).
        {"src/models/RadioModel.h", "rxFilter0LowPassReason",
         "the CH label's tooltip and the WIDE reason's second sentence, not a refusal"},
        // RADE reason: why a RADE slice has no working decoder
        // (radeStartReason, scanned above).
        {"src/models/SliceModel.h", "radeReason",
         "getter of the words radeStartReason made (scanned)"},
        {"src/models/SliceModel.cpp", "setRadeReason",
         "setter of the words radeStartReason made (scanned), or the Core's on a remote window"},
    };
    return sites;
}

// Text the station sends as a property value, which an app shows as sent
// (the link document's section 17): each accessory's connectionError (the
// Tuner Genius, the Power Genius and the RF-Kit amplifier), a receiver's
// nnrStatus and nnrLastError, the receive filters' rxFilter0Reason and
// rxFilter1Reason, and PureSignal's lastLoadError. NnrAdapter.cpp (the
// NNR explanation behind nnrStatus) is a reason source above.
const QList<ReasonSource>& propertyTextSources()
{
    static const QList<ReasonSource> sources{
        // failIdentityAdmission's parameter: this file's identity texts,
        // and the controller's (scanned below) through rejectIdentity.
        {"src/core/TgxlConnection.cpp", {}, {}, 4, {}, {QStringLiteral("reason")}, true},
        {"src/core/PgxlConnection.cpp", {}, {}, 4, {}, {QStringLiteral("reason")}, true},
        // The name an amplifier that is not an RF2K-S reports for itself.
        // `reason` is the identity refusal worded just above it.
        {"src/core/Rf2ksConnection.cpp", {}, {}, 2, {QStringLiteral("device")},
         {QStringLiteral("reason")}, true},
        // The name the device at the address reports for itself.
        {"src/core/StationTgxlController.cpp", {}, {}, 2, {QStringLiteral("product")}},
        // The name the device at the address reports for itself.
        {"src/core/StationPgxlController.cpp", {}, {}, 2, {QStringLiteral("product")}},
        // publish's error: Rf2ksConnection's connectionFailed reason, scanned
        // above.
        {"src/core/StationRfKitController.cpp", {}, {}, 1, {}, {QStringLiteral("reason")}},
        // Band names ("20m"), one or several joined with " + ". On the HL2
        // the slice the filter board is off for, by letter and band ("slice
        // B on WWV"), which RadioModel::republishAlexAdcSlices passes in.
        {"src/core/accessories/AlexController.cpp", {QStringLiteral("recomputeBpf")}, {}, 3,
         {QStringLiteral("bandLabel(s.currentBpfBand)"),
          QStringLiteral("bandList.join(QStringLiteral(\" + \"))"),
          QStringLiteral("m_switchBypassDetail[adc]")},
         // One band's name alone: the reason when one band is filtered.
         {QStringLiteral("bandLabel(s.currentBpfBand)")}},
        {"src/core/dsp/NnrSettings.h", {QStringLiteral("nnrLimitExplanation")}, {}, 4},
        {"src/models/SliceModel.cpp",
         {QStringLiteral("setActiveNr"), QStringLiteral("applyNnrSettings"),
          QStringLiteral("requestNnrDiagnostics"), QStringLiteral("restoreNnrSettings")},
         {}, 3},
        {"src/models/PureSignalSettings.cpp", {QStringLiteral("load")}, {}, 1},
        // The station TCI server's error (StationTciModel error), worded by
        // blockedReason. The port number and the Core's addresses.
        {"src/core/StationTciController.cpp", {}, {}, 1,
         {QStringLiteral("port"), QStringLiteral("stationAddresses.join(QStringLiteral(\", \"))"),
          // A setting's range (StationTciModel::settingsTable()).
          QStringLiteral("setting->min"), QStringLiteral("setting->max")},
         {// TciServer's own error text, kept for the Core's log line only.
          QStringLiteral("error"),
          // The unknown-client sentence is scanned in this file.
          QStringLiteral("unknownClientReason()")},
         true},

        // The 4O3A listener's error (fourO3AListenerError).
        {"src/core/SmartSdrApiListener.cpp", {}, {}, 1, {}, {}, true},
        // The settings save error (settingsSaveError) and the receive
        // layout notice (receiveLayoutRestoreMessage).
        {"src/models/RadioModel.cpp",
         {QStringLiteral("flushPendingSettingsSave"), QStringLiteral("bindReceiveLayoutSlices"),
          QStringLiteral("connectToRadioImpl"), QStringLiteral("prepareReceiveLayout"),
          QStringLiteral("completeReceiveLayoutStartup"),
          QStringLiteral("plainReceiveLayoutProblem"), QStringLiteral("withKeptLayout"),
          QStringLiteral("captureReceiveLayout"),
          QStringLiteral("activateRestoredRadeReceiveOwner"),
          QStringLiteral("radeAudioAwaitsReceiver"),
          // The notice for slices a smaller board cannot host.
          QStringLiteral("closedSliceSentence"),
          QStringLiteral("closeSlicesPastChannelLimit")},
         {}, 6,
         // activateRestoredRadeReceiveOwner's refusal, scanned here.
         {QStringLiteral("error")},
         // Sentences these functions word, joined by withKeptLayout; and
         // captureReceiveLayout's refusal, both scanned here. The closure
         // notice is closedSliceSentence's sentences (and the refusals
         // bindReceiveLayoutSlices words), joined; also scanned here.
         {QStringLiteral("withKeptLayout(refusals)"), QStringLiteral("withKeptLayout(notes)"),
          QStringLiteral("error"), QStringLiteral("m_sliceClosureNotice"),
          QStringLiteral("closureNotice")},
         true},
        // The Power Genius's efficiency: the device's own reading (a
        // number and a unit), passed on as it reports it.
        {"src/core/PgxlStatusGauges.cpp", {}, {}, 0, {}, {}, true},
        // iPhone app plan Task 39: txState's stopText. The limit ("3:00",
        // durationText) and a device's name as the Core numbers it.
        {"src/core/session/TransmitStateFacade.cpp",
         {QStringLiteral("timeOutText"), QStringLiteral("linkLostText"),
          QStringLiteral("micStarvedText"), QStringLiteral("revokedText"),
          QStringLiteral("takenOverText"), QStringLiteral("stationText")},
         // Fix wave M3: linkLostText is the watchdog's sentence (scanned in
         // RemoteTxWatchdog.cpp).
         {}, 7,
         {QStringLiteral("after"), QStringLiteral("deviceOrDefault(deviceName)"),
          QStringLiteral("leadingDevice(deviceName)"), QStringLiteral("leadingDevice(takerName)")}},
    };
    return sources;
}

// Functions that write a reason only by passing their QString* out-parameter
// on to a function scanned above: the call must be in the body, with the
// out-parameter among its arguments.
struct ForwardingSite {
    const char* file;
    const char* function;
    const char* callee;
};

const QList<ForwardingSite>& forwardingSites()
{
    static const QList<ForwardingSite> sites{
        {"src/core/RxChannel.cpp", "setNnrTuning", "NnrAdapter::apply"},
        {"src/core/RxChannel.cpp", "setNnrDiagnostics", "NnrAdapter::setDiagnostics"},
    };
    return sites;
}

QStringList sourceFiles(const QStringList& roots)
{
    QStringList files;
    for (const QString& root : roots) {
        QDirIterator it(sourcePath(root), {QStringLiteral("*.cpp"), QStringLiteral("*.h")},
                        QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            files.append(QDir(QStringLiteral(NEREUS_SOURCE_DIR)).relativeFilePath(it.next()));
        }
    }
    files.sort();
    return files;
}

bool scanned(const QString& file, const QString& function)
{
    for (const ForwardingSite& site : forwardingSites()) {
        if (file == QLatin1String(site.file) && function == QLatin1String(site.function)) {
            return true;
        }
    }
    for (const ReasonSource& source : reasonSources()) {
        if (file == QLatin1String(source.file)
            && (source.functions.isEmpty() || source.functions.contains(function))) {
            return true;
        }
    }
    return false;
}

bool appSide(const QString& file, const QString& function)
{
    for (const AppSideReason& site : appSideReasons()) {
        if (file == QLatin1String(site.file)
            && (QLatin1String(site.function).isEmpty() || function == QLatin1String(site.function))) {
            return true;
        }
    }
    return false;
}

// Every reason string inside a message: `reason` values at any depth
// (property.result carries one per property).
void collectReasons(const QJsonValue& value, QStringList* reasons)
{
    if (value.isObject()) {
        const QJsonObject object = value.toObject();
        for (auto it = object.begin(); it != object.end(); ++it) {
            if (it.key() == QLatin1String("reason") && it.value().isString()) {
                reasons->append(it.value().toString());
            } else {
                collectReasons(it.value(), reasons);
            }
        }
    } else if (value.isArray()) {
        for (const QJsonValue& item : value.toArray()) {
            collectReasons(item, reasons);
        }
    }
}

// The reason sites in `file` (its code `code`) this test neither scans
// nor names on the app side: a function whose name says it words a reason
// (…Reason, …Refusal, …ForStation, …FromStation, applyMirroredValue), a
// function that writes a reason through a QString* out-parameter
// (…reason, …Reason, …refusal), and a file that sends a reason to an app.
// `functions` counts the functions found by name.
QStringList unplacedReasonSites(const QString& file, const QString& code, int* functions = nullptr)
{
    static const QRegularExpression anyName(QStringLiteral("."));
    static const QRegularExpression reasonFunction(
        QStringLiteral("(Reason|Refusal|ForStation|FromStation)$|^applyMirroredValue$"));
    static const QRegularExpression outParameter(
        QStringLiteral("\\bQString\\s*\\*\\s*\\w*(?:[Rr]eason|[Rr]efusal)\\w*\\b"));
    static const QRegularExpression sender(QStringLiteral(
        "\\b(commandResult|sessionEnd|authResult|settingsReject|propertyResult|emitResult"
        "|dropPeer|sendRejected|sendAllocationResult|rejectAllocation|pairFail|sendPairFail)"
        "\\s*\\("));
    QStringList unplaced;
    for (const FunctionBody& function : functionsIn(code, anyName)) {
        const bool named = reasonFunction.match(function.name).hasMatch();
        const bool writesOut = outParameter.match(function.params).hasMatch();
        if (!named && !writesOut) {
            continue;
        }
        if (named && functions != nullptr) {
            ++*functions;
        }
        if (!scanned(file, function.name) && !appSide(file, function.name)) {
            unplaced.append(file + QStringLiteral(": ") + function.name
                            + (named ? QString() : QStringLiteral(" (writes a reason it is given)")));
        }
    }
    if (sender.match(code).hasMatch() && !scanned(file, QString()) && !appSide(file, QString())) {
        unplaced.append(file + QStringLiteral(": sends a reason"));
    }
    return unplaced;
}

// Checks every source of `sources` as reasons: the problems found, and
// how many reasons were checked. `failures` also gets a line for a source
// with fewer reasons than it promises, or a named function not found.
int checkReasonSources(const QList<ReasonSource>& sources, QStringList* failures)
{
    static const QRegularExpression anyName(QStringLiteral("."));
    int checked = 0;
    for (const ReasonSource& source : sources) {
        const QString code = codeOf(sourcePath(QString::fromLatin1(source.file)));
        if (code.isEmpty()) {
            failures->append(QStringLiteral("%1: not read").arg(QLatin1String(source.file)));
            continue;
        }
        QList<ReasonText> found;
        if (source.functions.isEmpty()) {
            found = reasonsIn(code);
        } else {
            QStringList seen;
            for (const FunctionBody& function : functionsIn(code, anyName)) {
                if (!source.functions.contains(function.name)) {
                    continue;
                }
                seen.append(function.name);
                for (const ReasonText& reason : reasonsIn(function.body)) {
                    const bool known = std::any_of(
                        found.cbegin(), found.cend(),
                        [&reason](const ReasonText& r) { return r.text == reason.text; });
                    if (!known) {
                        found.append(reason);
                    }
                }
            }
            for (const QString& function : source.functions) {
                if (!seen.contains(function)) {
                    failures->append(QStringLiteral("%1: %2 not found")
                                         .arg(QLatin1String(source.file), function));
                }
            }
        }
        int reasons = 0;
        for (const ReasonText& reason : found) {
            if (source.positionedOnly && !reason.positioned) {
                continue;
            }
            const bool exempt = std::any_of(
                source.notReasons.cbegin(), source.notReasons.cend(),
                [&reason](const QString& start) { return reason.text.startsWith(start); });
            if (exempt) {
                continue;
            }
            ++reasons;
            for (const QString& problem :
                 problemsOf(reason, source.plainInserts, source.forwards)) {
                failures->append(QStringLiteral("%1: %2").arg(QLatin1String(source.file), problem));
            }
        }
        if (reasons < source.atLeast) {
            failures->append(QStringLiteral("%1: %2 reasons, fewer than %3")
                                 .arg(QLatin1String(source.file))
                                 .arg(reasons)
                                 .arg(source.atLeast));
        }
        checked += reasons;
    }
    return checked;
}

} // namespace

class TestStationReasonWording : public QObject {
    Q_OBJECT

private slots:
    void theCheckCatchesDeveloperWording()
    {
        // The rule, on the reasons this task retired.
        for (const char* old :
             {"Remote PureSignal actuation requires R4 transmit support.",
              "Model application requires the current selection revision.",
              "missing sliceId argument", "disconnectTgxl takes no arguments",
              "dspAssets.chunk has invalid or missing fields.",
              "SliceModel::%1 has no inbound mirror translation",
              "C-Tune centre is invalid for this stream's cohosts",
              "session display budget exceeded", "handshake deadline expired",
              "key is not Station-scoped",
              // R-R3-21: the Core is the Core, not the station.
              "The station sets this itself; it cannot be changed from here.",
              "This Core has no TCI server for the station.",
              "Transmit configuration is unavailable on this receive-only station.",
              // JJ's rule: "yet" promises a future.
              "PureSignal cannot be run from a remote window yet.",
              "This device's microphone is not connected to the Core yet."}) {
            QVERIFY2(!wordingProblemIn(QString::fromUtf8(old)).isEmpty(), old);
        }
        for (const char* plain :
             {"PureSignal cannot be run from a remote window.",
              "The Core could not read this request.",
              "Update this app to set up the Power Genius on this Core.",
              "Choose an SWR protection limit from %1 to %2.",
              // The ham sense of "station" stays.
              "Operating the station's amplifier or tuner waits for remote transmit. "
              "This Core is receive-only."}) {
            QVERIFY2(wordingProblemIn(QString::fromUtf8(plain)).isEmpty(), plain);
        }
    }

    void theScanReadsOneWordAndInsertedReasons()
    {
        // A one-word reason is not a sentence an operator can act on.
        const QStringList oneWord = wordingProblemsInCode(
            QStringLiteral("emitResult(verb, id, false, QStringLiteral(\"unimplemented\"), {});"),
            {});
        QVERIFY2(oneWord.join(QLatin1Char('|')).contains(QStringLiteral("unimplemented")),
                 qPrintable(oneWord.join(QLatin1Char('|'))));
        const QStringList assigned = wordingProblemsInCode(
            QStringLiteral("*reason = QStringLiteral(\"EINVAL\");"), {});
        QVERIFY2(assigned.join(QLatin1Char('|')).contains(QStringLiteral("EINVAL")),
                 qPrintable(assigned.join(QLatin1Char('|'))));
        // What .arg() inserts into a reason is part of the reason: a
        // developer word inserted as a literal, and an expression nobody
        // has said inserts plain words.
        const QStringList insertedLiteral = wordingProblemsInCode(
            QStringLiteral("dropPeer(t, QStringLiteral(\"The Core stopped %1.\")"
                           ".arg(QStringLiteral(\"sessionEpoch\")), true);"),
            {});
        QVERIFY2(insertedLiteral.join(QLatin1Char('|')).contains(QStringLiteral("sessionEpoch")),
                 qPrintable(insertedLiteral.join(QLatin1Char('|'))));
        const QStringList insertedName = wordingProblemsInCode(
            QStringLiteral("emitResult(verb, id, false, QStringLiteral(\"The Core refused %1.\")"
                           ".arg(QString::fromUtf8(verb)), {});"),
            {});
        QVERIFY2(insertedName.join(QLatin1Char('|')).contains(QStringLiteral("fromUtf8(verb)")),
                 qPrintable(insertedName.join(QLatin1Char('|'))));
        // Named as plain, the same insertion passes.
        QVERIFY(wordingProblemsInCode(
                    QStringLiteral("emitResult(verb, id, false, QStringLiteral(\"The Core refused %1.\")"
                                   ".arg(count), {});"),
                    {QStringLiteral("count")})
                    .isEmpty());
        // The discriminator of a conditional reason remains a wire verb;
        // only the chosen arm is operator text. A bad arm must still fail.
        const QString conditional = QStringLiteral(
            "emitResult(verb, id, false, verb == \"txModMonitor.reset\" ? "
            "QStringLiteral(\"The Core cannot do this.\") : "
            "QStringLiteral(\"The session expired.\"), {});");
        const QStringList conditionalProblems = wordingProblemsInCode(conditional, {});
        QVERIFY(!conditionalProblems.join(QLatin1Char('|')).contains(
            QStringLiteral("txModMonitor.reset")));
        QVERIFY(conditionalProblems.join(QLatin1Char('|')).contains(
            QStringLiteral("The session expired.")));
        // The media helper's first argument identifies a slice. Its third
        // argument reaches sendIqContext, including one-word refusals.
        const QStringList mediaProblems = wordingProblemsInCode(
            QStringLiteral("fail(sliceId, stream, QStringLiteral(\"unavailable\"));"), {});
        QVERIFY(!mediaProblems.join(QLatin1Char('|')).contains(QStringLiteral("sliceId")));
        QVERIFY(mediaProblems.join(QLatin1Char('|')).contains(QStringLiteral("unavailable")));
    }

    // TX diagnostics lane: the unkey event lines' literals are exempt as
    // log text only while they are: each is written in unkeyEventLines and
    // nowhere else in the file, and the lines that function returns go to
    // the log and nowhere else (one caller in src, logUnkeyStats, which
    // logs each with qCInfo).
    void unkeyEventLinesAreLogOnly()
    {
        const QString file = QStringLiteral("src/core/session/media/DaemonMediaController.cpp");
        const QString code = codeOf(sourcePath(file));
        QVERIFY(!code.isEmpty());
        static const QRegularExpression eventLines(QStringLiteral("^unkeyEventLines$"));
        static const QRegularExpression logger(QStringLiteral("^logUnkeyStats$"));
        const QList<FunctionBody> bodies = functionsIn(code, eventLines);
        QCOMPARE(bodies.size(), 1);
        const QString& body = bodies.first().body;
        for (const QString& text : unkeyEventLogText()) {
            const QString opening = QLatin1Char('"') + text;
            const QString literal = opening + QLatin1Char('"');
            QVERIFY2(body.contains(literal), qPrintable(text + QStringLiteral(": not in the function")));
            // notReasons exempts by start: no literal in the file may start
            // with this text and go on past it ...
            QVERIFY2(code.count(opening) == code.count(literal),
                     qPrintable(text + QStringLiteral(": a longer literal starts with it")));
            // ... nor be this exact text outside unkeyEventLines.
            QVERIFY2(code.count(literal) == body.count(literal),
                     qPrintable(text + QStringLiteral(": written outside unkeyEventLines")));
        }

        // The callers: in src, only logUnkeyStats, which logs each line.
        // Any mention counts (a call, or a reference passed on without one).
        int calls = 0;
        QDirIterator it(sourcePath(QStringLiteral("src")),
                        {QStringLiteral("*.cpp"), QStringLiteral("*.h"), QStringLiteral("*.mm")},
                        QDir::Files, QDirIterator::Subdirectories);
        static const QRegularExpression call(QStringLiteral("\\bunkeyEventLines\\b"));
        while (it.hasNext()) {
            const QString path = it.next();
            const QString source = codeOf(path);
            const bool own = path.endsWith(file);
            int here = static_cast<int>(source.count(call));
            if (own) {
                here -= 1;   // the definition
            } else if (path.endsWith(QStringLiteral("DaemonMediaController.h"))) {
                here -= 1;   // the declaration
            }
            QVERIFY2(here == 0 || own, qPrintable(path + QStringLiteral(" calls unkeyEventLines")));
            calls += here;
        }
        QCOMPARE(calls, 1);
        const QList<FunctionBody> loggers = functionsIn(code, logger);
        QCOMPARE(loggers.size(), 1);
        static const QRegularExpression logged(QStringLiteral(
            "const QStringList events = unkeyEventLines\\([^;]*\\);\\s*"
            "for \\(const QString& event : events\\) \\{\\s*"
            "qCInfo\\(lcDaemonMedia\\)\\.noquote\\(\\) << event;\\s*\\}"));
        const QString& loggerBody = loggers.first().body;
        QVERIFY2(logged.match(loggerBody).hasMatch(),
                 "logUnkeyStats no longer only logs the unkey event lines");
        // And nothing else in it touches the lines: `events` only at its
        // declaration and the loop, `event` only in the loop.
        static const QRegularExpression eventsName(QStringLiteral("\\bevents\\b"));
        static const QRegularExpression eventName(QStringLiteral("\\bevent\\b"));
        QVERIFY2(loggerBody.count(eventsName) == 2,
                 "logUnkeyStats uses the unkey event lines past logging them");
        QVERIFY2(loggerBody.count(eventName) == 2,
                 "logUnkeyStats uses an unkey event line past logging it");
    }

    void everyStationReasonIsPlain()
    {
        QStringList failures;
        const int checked = checkReasonSources(reasonSources(), &failures);
        // QtTest cuts a long message short; each failure gets its own line.
        for (const QString& failure : std::as_const(failures)) {
            qWarning().noquote() << failure;
        }
        QVERIFY2(failures.isEmpty(), qPrintable(failures.join(QLatin1Char('\n'))));
        QVERIFY2(checked >= 250, qPrintable(QString::number(checked)));
    }

    void forwardingSitesPassTheirReasonOn()
    {
        static const QRegularExpression anyName(QStringLiteral("."));
        int checked = 0;
        for (const ForwardingSite& site : forwardingSites()) {
            const QString code = codeOf(sourcePath(QString::fromLatin1(site.file)));
            bool passed = false;
            for (const FunctionBody& function : functionsIn(code, anyName)) {
                if (function.name != QLatin1String(site.function)) {
                    continue;
                }
                const QString callee = QLatin1String(site.callee) + QLatin1Char('(');
                const qsizetype call = function.body.indexOf(callee);
                if (call < 0) {
                    continue;
                }
                const qsizetype open = call + callee.size() - 1;
                const qsizetype close =
                    closingOf(function.body, open, QLatin1Char('('), QLatin1Char(')'));
                for (const QString& argument :
                     splitTopLevel(function.body.mid(open + 1, close - open - 2))) {
                    passed = passed || argument.trimmed() == QStringLiteral("reason");
                }
            }
            QVERIFY2(passed, qPrintable(QStringLiteral("%1: %2 does not pass its reason to %3")
                                            .arg(QLatin1String(site.file),
                                                 QLatin1String(site.function),
                                                 QLatin1String(site.callee))));
            const QString calleeClass =
                QString::fromLatin1(site.callee).section(QStringLiteral("::"), 0, 0);
            QVERIFY2(scanned(QStringLiteral("src/core/dsp/%1.cpp").arg(calleeClass), QString()),
                     site.callee);
            ++checked;
        }
        QCOMPARE(checked, 2);
    }

    void notchConstantsArePlain()
    {
        // RadioModel.cpp's two notch refusals are file-scope constants the
        // function scan names as forwards; their words are checked here.
        const QString code = codeOf(sourcePath(QStringLiteral("src/models/RadioModel.cpp")));
        int checked = 0;
        for (const char* name : {"kUnknownNotchReason", "kNotchListBusyReason"}) {
            const QRegularExpression definition(
                QStringLiteral("\\bconst QString %1\\s*=\\s*QStringLiteral\\(\\s*\"([^\"]*)\"")
                    .arg(QLatin1String(name)));
            const QRegularExpressionMatch match = definition.match(code);
            QVERIFY2(match.hasMatch(), name);
            const QString problem = wordingProblemIn(match.captured(1));
            QVERIFY2(problem.isEmpty(), qPrintable(match.captured(1) + QStringLiteral(" [")
                                                   + problem + QLatin1Char(']')));
            ++checked;
        }
        QCOMPARE(checked, 2);
    }

    void everyStationPropertyTextIsPlain()
    {
        // Text sent as a property value is shown as sent too.
        QStringList failures;
        const int checked = checkReasonSources(propertyTextSources(), &failures);
        // QtTest cuts a long message short; each failure gets its own line.
        for (const QString& failure : std::as_const(failures)) {
            qWarning().noquote() << failure;
        }
        QVERIFY2(failures.isEmpty(), qPrintable(failures.join(QLatin1Char('\n'))));
        QVERIFY2(checked >= 25, qPrintable(QString::number(checked)));
    }

    void anUnlistedReasonSiteFails()
    {
        // A function in a file this test does not list, writing a reason
        // through a QString* out-parameter, fails whatever it is called.
        const QString planted = QStringLiteral(
            "bool PlantedModel::applyThing(int value, QString* reason)\n"
            "{\n    if (value < 0) { *reason = QStringLiteral(\"The Core refused it.\"); }\n"
            "    return value >= 0;\n}\n");
        const QStringList unplaced =
            unplacedReasonSites(QStringLiteral("src/core/PlantedModel.cpp"), planted);
        QVERIFY2(unplaced.join(QLatin1Char('|')).contains(QStringLiteral("applyThing")),
                 qPrintable(unplaced.join(QLatin1Char('|'))));
        // Named on the app side, it is placed.
        QVERIFY(unplacedReasonSites(QStringLiteral("src/core/session/StationClient.cpp"), planted)
                    .isEmpty());
        // The new desktop fallbacks are named individually. A future
        // IStationLink reason remains visible to the guard.
        QVERIFY(appSide(QStringLiteral("src/core/session/IStationLink.h"),
                        QStringLiteral("modMonitorUnavailableReason")));
        QVERIFY(!appSide(QStringLiteral("src/core/session/IStationLink.h"),
                         QStringLiteral("newUnavailableReason")));
        QVERIFY(unplacedReasonSites(
                    QStringLiteral("src/core/session/IStationLink.h"),
                    QStringLiteral("QString newUnavailableReason() { return "
                                   "QStringLiteral(\"The session failed.\"); }"))
                    .contains(QStringLiteral("src/core/session/IStationLink.h: "
                                             "newUnavailableReason")));
    }

    void everyReasonSiteIsScannedOrOnTheAppSide()
    {
        QStringList unplaced;
        int functions = 0;
        for (const QString& file :
             sourceFiles({QStringLiteral("src/core"), QStringLiteral("src/models")})) {
            const QString code = codeOf(sourcePath(file));
            unplaced.append(unplacedReasonSites(file, code, &functions));
        }
        QVERIFY2(functions >= 30, qPrintable(QString::number(functions)));
        QVERIFY2(unplaced.isEmpty(), qPrintable(unplaced.join(QLatin1Char('\n'))));
    }

    void pairFailReasonsSpeakOfTheCore()
    {
        // iPhone app Task 14 (R-IOS-08): every reason pair.fail carries
        // (StationServer's sendPairFail, and handlePairConfirm's refusal
        // helper, which passes its reason there) is plain and names the
        // Core, the word the operator knows it by.
        static const QRegularExpression anyName(QStringLiteral("."));
        static const QRegularExpression literal(
            QStringLiteral("^\\s*QStringLiteral\\(\\s*\"((?:[^\"\\\\]|\\\\.)*)\"\\s*\\)\\s*$"));
        const QString code = codeOf(sourcePath(QStringLiteral("src/core/session/StationServer.cpp")));
        QVERIFY(!code.isEmpty());
        QStringList reasons;
        for (const FunctionBody& function : functionsIn(code, anyName)) {
            QList<QPair<QString, int>> callees{{QStringLiteral("sendPairFail("), 1}};
            if (function.name == QLatin1String("handlePairConfirm")) {
                callees.append({QStringLiteral("refuse("), 0});
            }
            for (const auto& [callee, index] : callees) {
                qsizetype at = function.body.indexOf(callee);
                while (at >= 0) {
                    const qsizetype open = at + callee.size() - 1;
                    const qsizetype close =
                        closingOf(function.body, open, QLatin1Char('('), QLatin1Char(')'));
                    const QStringList arguments =
                        splitTopLevel(function.body.mid(open + 1, close - open - 2));
                    if (index < arguments.size()) {
                        const QRegularExpressionMatch match = literal.match(arguments.at(index));
                        if (match.hasMatch()) {
                            reasons.append(match.captured(1));
                        }
                    }
                    at = function.body.indexOf(callee, close);
                }
            }
        }
        QVERIFY2(reasons.size() >= 12, qPrintable(QString::number(reasons.size())));
        for (const QString& reason : std::as_const(reasons)) {
            QVERIFY2(wordingProblemIn(reason).isEmpty(), qPrintable(reason));
            QVERIFY2(reason.contains(QStringLiteral("Core")), qPrintable(reason));
        }
    }

    void severalDevicesSentencesArePlain()
    {
        // iPhone app Task 71 (R-IOS-02): the sentences the Core sends when
        // several devices share it are plain and speak of the Core; names
        // are the operator's own words and are validated as names, never
        // held to the wording rules ("Grant's iPhone" is a name).
        const QString code = codeOf(sourcePath(QStringLiteral("src/core/session/StationServer.cpp")));
        QVERIFY(!code.isEmpty());
        for (const QString& sentence :
             {QStringLiteral("The Core already has four devices connected."),
              QStringLiteral("The Core is full. Update NereusSDR to take a device's place, or try again later."),
              QStringLiteral("This device connected again."),
              QStringLiteral("This device left the Core.")}) {
            QVERIFY2(code.contains(QLatin1Char('"') + sentence + QLatin1Char('"')),
                     qPrintable(sentence));
            QVERIFY2(wordingProblemIn(sentence).isEmpty(), qPrintable(sentence));
            QVERIFY2(OperatorWording::coreCalledStationIn(sentence).isEmpty(),
                     qPrintable(sentence));
        }
        const QString leave = QStringLiteral("The request to leave the Core was not understood.");
        QVERIFY(codeOf(sourcePath(QStringLiteral("src/core/session/SessionCommandDispatcher.cpp")))
                    .contains(QLatin1Char('"') + leave + QLatin1Char('"')));
        QVERIFY2(wordingProblemIn(leave).isEmpty(), qPrintable(leave));
        // A device's name is the operator's own words: the term list would
        // refuse "Grant's iPhone", so a sentence that carries a name is
        // checked with the name set aside, and the name as a name.
        const QString name = QStringLiteral("Grant's iPhone");
        QVERIFY(!OperatorWording::isPlain(name));
        QVERIFY(DeviceStore::isValidName(name));
        QString named = QStringLiteral("%1 connected to the Core.").arg(name);
        named.remove(name);
        QVERIFY2(wordingProblemIn(named).isEmpty(), qPrintable(named));
    }

    void receiverSentencesArePlain()
    {
        // iPhone app Task 74 (R-IOS-30; the several-devices design, section
        // 17's list): the reason of confirm.request and notice, each
        // chooser `why`, the three strings of `change`, and the refusals of
        // the anchor, take and older-window rules. Names are the operator's
        // own words and are set aside; "Grant's iPhone" is embedded to prove
        // the sentence around it is what is checked.
        const QString receivers =
            codeOf(sourcePath(QStringLiteral("src/core/session/StationReceivers.cpp")));
        const QString planner =
            codeOf(sourcePath(QStringLiteral("src/core/session/ReceiverPlanner.cpp")));
        QVERIFY(!receivers.isEmpty());
        QVERIFY(!planner.isEmpty());
        const QString name = QStringLiteral("Grant's iPhone");
        const QStringList inReceivers{
            QStringLiteral("Waiting for you to confirm."),
            QStringLiteral("That question is no longer open. Make the change again."),
            QStringLiteral("That choice is not in the list. Make the change again."),
            QStringLiteral("What this change reaches has changed. Make the change again."),
            QStringLiteral("That can no longer be taken back."),
            QStringLiteral("All the radio's slices are in use. Try again when another device "
                           "closes one."),
            QStringLiteral("All the radio's receivers are in use. Try again when another device "
                           "frees one."),
            QStringLiteral("This panadapter shows %1's receiver. Its C-Tune setting is %1's."),
            QStringLiteral("This change would affect %1. Update NereusSDR to confirm changes that "
                           "affect other devices."),
            QStringLiteral("%1 took the receiver this app was using. Update NereusSDR to share "
                           "the Core."),
            QStringLiteral("The radio's receivers are in use by %1."),
            QStringLiteral("%1 moved their panadapter. Your slice %2 moved to another receiver."),
            QStringLiteral("%1 moved their panadapter. Your slice %2 closed: no receiver was "
                           "free."),
            QStringLiteral("%1 took the receiver your slice %2 was on."),
            QStringLiteral("%1 took your slice %2."),
            QStringLiteral("You were away for more than 3 minutes. Your slices are back."),
            QStringLiteral("%1 of your slices could not be restored: all the radio's receivers "
                           "are in use."),
            QStringLiteral("Receiver %1"),
        };
        const QStringList inPlanner{
            QStringLiteral("Your slice %1 would close."),
            QStringLiteral("Your slices %1 would close."),
            QStringLiteral("Your panadapter already uses this receiver."),
            // planRestoreAfterClosing's own refusals. Its remaining refusal
            // comes from SliceStreamAllocator, scanned as a reason source.
            QStringLiteral("There are no saved slices to restore."),
            QStringLiteral("That receiver changed since you asked."),
            QStringLiteral("The Core must keep its last receiver slice."),
            QStringLiteral("All slice places are in use."),
            QStringLiteral("The receiver has no usable sample rate."),
            QStringLiteral("A saved slice has no usable frequency."),
        };
        const auto check = [&name](const QString& sentence) {
            QString filled = sentence;
            filled.replace(QStringLiteral("%1"), name).replace(QStringLiteral("%2"),
                                                               QStringLiteral("B and C"));
            // The name set aside, the sentence is held to the rule.
            filled.remove(name);
            return wordingProblemIn(filled);
        };
        const auto flat = [](const QString& code) {
            // Adjacent literals split over lines, joined.
            QString joined = code;
            joined.replace(QRegularExpression(QStringLiteral("\"\\s*\n\\s*\"")), QString());
            return joined;
        };
        const QString receiverCode = flat(receivers);
        const QString plannerCode = flat(planner);
        for (const QString& sentence : inReceivers) {
            QVERIFY2(receiverCode.contains(QLatin1Char('"') + sentence + QLatin1Char('"')),
                     qPrintable(sentence));
            QVERIFY2(check(sentence).isEmpty(), qPrintable(sentence + QStringLiteral(" [")
                                                           + check(sentence) + QLatin1Char(']')));
            QVERIFY2(OperatorWording::coreCalledStationIn(sentence).isEmpty(),
                     qPrintable(sentence));
        }
        for (const QString& sentence : inPlanner) {
            QVERIFY2(plannerCode.contains(QLatin1Char('"') + sentence + QLatin1Char('"')),
                     qPrintable(sentence));
            QVERIFY2(check(sentence).isEmpty(), qPrintable(sentence));
        }
        // The bands of `change` ("40 m", "20 m") are the band's own label.
        QVERIFY2(wordingProblemIn(QStringLiteral("Go to 20 m")).isEmpty(),
                 qPrintable(wordingProblemIn(QStringLiteral("Go to 20 m"))));
    }

    void sharedSettingSentencesArePlain()
    {
        // iPhone app Task 75 (R-IOS-30; the several-devices design,
        // sections 7.3 and 7.4, rulings 5.11a, 7.4 to 7.6): the refusals
        // of the confirm step's target rules, the on-air refusal, the
        // notices settingChanged and antennaKept, and the words of
        // `change`. Names are the operator's own words and are set aside;
        // "Grant's iPhone" is embedded to prove the sentence around it is
        // what is checked.
        const auto flat = [](const QString& code) {
            QString joined = code;
            joined.replace(QRegularExpression(QStringLiteral("\"\\s*\n\\s*\"")), QString());
            return joined;
        };
        const QString shared =
            flat(codeOf(sourcePath(QStringLiteral("src/core/session/StationSharedSettings.cpp"))));
        const QString receivers =
            flat(codeOf(sourcePath(QStringLiteral("src/core/session/StationReceivers.cpp"))));
        QVERIFY(!shared.isEmpty());
        const QString name = QStringLiteral("Grant's iPhone");
        const QStringList inShared{
            QStringLiteral("That setting changed since you asked. Make the change again."),
            QStringLiteral("%1 is on the air. Try again when they stop."),
            QStringLiteral("%1 changed %2 from %3 to %4."),
            QStringLiteral("Your slice %1 moved to another receiver."),
            QStringLiteral("Your slices %1 moved to another receiver."),
            QStringLiteral("Your slice %1 closed: no receiver was free."),
            QStringLiteral("Your slices %1 closed: no receiver was free."),
            QStringLiteral("Your slice %1 pauses while the radio transmits."),
            QStringLiteral("Your slices %1 pause while the radio transmits."),
            QStringLiteral("The antenna stays on %1 while %2 listens on it."),
            QStringLiteral("The antenna stays on %1 while %2 listen on it."),
            QStringLiteral("Attenuator, %1"),
            QStringLiteral("Preamp, %1"),
            QStringLiteral("Automatic attenuator, %1"),
            QStringLiteral("Receive antenna"),
            QStringLiteral("Receive-only antenna"),
            QStringLiteral("Receive on the transmit antenna"),
            QStringLiteral("Transmit antenna"),
            QStringLiteral("Diversity"),
            QStringLiteral("Noise blanker, %1"),
            QStringLiteral("Noise blanker settings, %1"),
            QStringLiteral("PureSignal"),
            QStringLiteral("Notch auto-widening"),
            QStringLiteral("Amplifier"),
            QStringLiteral("Sample rate, %1"),
            QStringLiteral("Band filters, %1"),
            QStringLiteral("%1, %2 modes"),
            QStringLiteral("Tuner antenna"),
            QStringLiteral("Tuner bypass"),
            QStringLiteral("Transmit interlock"),
            QStringLiteral("Amplifier power limit"),
        };
        const auto check = [&name](const QString& sentence) {
            QString filled = sentence;
            filled.replace(QStringLiteral("%1"), name)
                .replace(QStringLiteral("%2"), QStringLiteral("Attenuator, ADC 1"))
                .replace(QStringLiteral("%3"), QStringLiteral("0 dB"))
                .replace(QStringLiteral("%4"), QStringLiteral("20 dB"));
            filled.remove(name);
            return wordingProblemIn(filled);
        };
        for (const QString& sentence : inShared) {
            QVERIFY2(shared.contains(QLatin1Char('"') + sentence + QLatin1Char('"')),
                     qPrintable(sentence));
            QVERIFY2(check(sentence).isEmpty(), qPrintable(sentence + QStringLiteral(" [")
                                                           + check(sentence) + QLatin1Char(']')));
            QVERIFY2(OperatorWording::coreCalledStationIn(sentence).isEmpty(),
                     qPrintable(sentence));
        }
        // Ruling 7.5, written with the confirm step's other answers.
        const QString expired = QStringLiteral("That question has expired. Make the change again.");
        QVERIFY(receivers.contains(QLatin1Char('"') + expired + QLatin1Char('"')));
        QVERIFY2(wordingProblemIn(expired).isEmpty(), qPrintable(expired));
        // A whole notice as sent, a name in it.
        QString told = QStringLiteral("%1 changed Sample rate, Receiver 1 from 192 kHz to 96 kHz. "
                                      "Your slice B moved to another receiver.")
                           .arg(name);
        told.remove(name);
        QVERIFY2(wordingProblemIn(told).isEmpty(), qPrintable(told));
    }

    void everyReasonTheFixturesRecordIsPlain()
    {
        const QDir sessions(QStringLiteral(NEREUS_SOURCE_DIR "/tests/data/link/v1/sessions"));
        const QStringList files = sessions.entryList({QStringLiteral("*.json")}, QDir::Files);
        QVERIFY(files.size() >= 25);
        QStringList failures;
        int checked = 0;
        for (const QString& name : files) {
            QFile file(sessions.filePath(name));
            QVERIFY(file.open(QIODevice::ReadOnly));
            const QJsonObject fixture = QJsonDocument::fromJson(file.readAll()).object();
            for (const QJsonValue& step : fixture.value(QStringLiteral("steps")).toArray()) {
                const QJsonObject object = step.toObject();
                if (object.value(QStringLiteral("from")).toString() != QLatin1String("station")) {
                    continue;
                }
                QStringList reasons;
                collectReasons(object.value(QStringLiteral("message")), &reasons);
                for (const QString& reason : reasons) {
                    // Empty on success; "$..." is a placeholder the matcher fills.
                    if (reason.isEmpty() || reason.startsWith(QLatin1Char('$'))) {
                        continue;
                    }
                    ++checked;
                    const QString problem = wordingProblemIn(reason);
                    if (!problem.isEmpty()) {
                        failures.append(QStringLiteral("%1: \"%2\" [%3]").arg(name, reason, problem));
                    }
                }
            }
        }
        QVERIFY2(checked >= 60, qPrintable(QString::number(checked)));
        QVERIFY2(failures.isEmpty(), qPrintable(failures.join(QLatin1Char('\n'))));
    }
};

QTEST_GUILESS_MAIN(TestStationReasonWording)
#include "tst_station_reason_wording.moc"
