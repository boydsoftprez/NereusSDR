// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_link_surface_manifest.cpp  (NereusSDR)
// =================================================================
//
// iPhone app plan, Task 1 (R-IOS-01): the drift guard for the link's
// surface. tests/data/link/v1/surface.json must equal LinkSurface::
// capture() on the tree as it is; a change to the wire that does not
// regenerate the file fails here, naming what changed.
//
// Beside that comparison, the declared tables the capture reads are held
// to the code that routes and validates:
//
//   - allKinds() against the SessionMessageKind declaration;
//   - each kind's recorded keys against every key SessionMessages::encode
//     can write for it, read from its source, conditional keys included;
//   - verbSpecs() against the literals SessionCommandDispatcher.cpp routes
//     on and the concrete verbs each prefix family's handler accepts, and
//     against a live dispatch of every verb and of verbs it does not list;
//   - the media control operations against the operations each end
//     dispatches on and the exact key sets they check or build;
//   - the limits held in file-scope constants against their source lines.
//
// Regenerate the file with tst_link_surface_manifest_regen
// (NEREUS_LINK_REGEN_OUT=tests/data/link/v1) and read the diff before
// committing it.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-04  J.J. Boyd / KG4VCF  Operation-scoped media fields, codec
//                                    combinations and legacy nested shapes.
//                                    AI-assisted via OpenAI Codex.
//   2026-10-04  J.J. Boyd / KG4VCF  Complete capability declaration coverage;
//                                    retain honest optional live values.
//                                    AI-assisted via OpenAI Codex.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Task 1 (R-IOS-01): link
//                                    surface drift guard. AI-assisted
//                                    transformation via Anthropic Claude
//                                    Code.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Part A fix wave (R-IOS-01):
//                                    message keys held to the encoder's
//                                    source; each key's JSON type.
//                                    AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <QCoreApplication>
#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>

#include <memory>

#include "core/AppSettings.h"
#include "core/session/MirrorSchema.h"
#include "core/session/SessionCommandDispatcher.h"
#include "core/session/SessionMessages.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "core/session/media/DisplayBudget.h"
#include "core/session/media/DisplayExtras.h"
#include "core/session/media/RemoteAudioContext.h"
#include "core/session/media/RemoteSpectrumContext.h"
#include "models/RadioModel.h"

#include "LinkSurface.h"
#include "fakes/ConnectableRadioModel.h"

using namespace NereusSDR;
using NereusSDR::Test::ConnectableRadioModel;
using NereusSDR::Test::LinkSurface;

namespace {

QString readSource(const QString& relative)
{
    QFile file(QStringLiteral(NEREUS_SOURCE_DIR "/") + relative);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }
    return QString::fromUtf8(file.readAll());
}

QSet<QString> matches(const QString& text, const QString& pattern, int group = 1)
{
    QSet<QString> found;
    const QRegularExpression re(pattern);
    auto it = re.globalMatch(text);
    while (it.hasNext()) {
        found.insert(it.next().captured(group));
    }
    return found;
}

QString joined(QSet<QString> values)
{
    QStringList list(values.cbegin(), values.cend());
    list.sort();
    return list.join(QStringLiteral(", "));
}

QSet<QString> stringSet(const QJsonArray& array)
{
    QSet<QString> out;
    for (const QJsonValue& v : array) {
        out.insert(v.toString());
    }
    return out;
}

// Read only literal first arguments to the three entry constructors inside
// StationCapabilities::toUpdates(). Tokenizing keeps comments and unrelated
// string contents out of the inventory, and balances only code braces.
QHash<QString, QString> capabilityDeclarations(const QString& source, QString* error)
{
    error->clear();
    static const QRegularExpression token(QString::fromLatin1(
        R"re(//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'|[A-Za-z_][A-Za-z0-9_]*|::|[^\s])re"));
    QStringList tokens;
    QRegularExpressionMatchIterator it = token.globalMatch(source);
    while (it.hasNext()) {
        const QString text = it.next().captured();
        if (!text.startsWith(QStringLiteral("//"))
            && !text.startsWith(QStringLiteral("/*"))) {
            tokens.append(text);
        }
    }
    const QStringList signature{QStringLiteral("StationCapabilities"), QStringLiteral("::"),
                                QStringLiteral("toUpdates"), QStringLiteral("("),
                                QStringLiteral(")"), QStringLiteral("const"), QStringLiteral("{")};
    qsizetype begin = -1;
    for (qsizetype i = 0; i + signature.size() <= tokens.size(); ++i) {
        if (tokens.mid(i, signature.size()) == signature) {
            if (begin >= 0) {
                *error = QStringLiteral("StationCapabilities::toUpdates found more than once");
                return {};
            }
            begin = i + signature.size();
        }
    }
    if (begin < 0) {
        *error = QStringLiteral("StationCapabilities::toUpdates not found");
        return {};
    }
    const QHash<QString, QString> kinds{{QStringLiteral("intEntry"), QStringLiteral("i64")},
                                        {QStringLiteral("boolEntry"), QStringLiteral("bool")},
                                        {QStringLiteral("stringEntry"), QStringLiteral("utf8")}};
    static const QRegularExpression literalName(
        QString::fromLatin1(R"re(^"([A-Za-z][A-Za-z0-9]*)"$)re"));
    QHash<QString, QString> declarations;
    int depth = 1;
    for (qsizetype i = begin; i < tokens.size(); ++i) {
        const QString text = tokens.at(i);
        if (text == QStringLiteral("{")) {
            ++depth;
        } else if (text == QStringLiteral("}")) {
            if (--depth == 0) {
                return declarations;
            }
        }
        if (!kinds.contains(text) || i + 3 >= tokens.size()
            || tokens.at(i + 1) != QStringLiteral("(")) {
            continue;
        }
        const QRegularExpressionMatch name = literalName.match(tokens.at(i + 2));
        if (!name.hasMatch() || tokens.at(i + 3) != QStringLiteral(",")) {
            *error = QStringLiteral("entry constructor has a nonliteral capability name");
            return {};
        }
        if (declarations.contains(name.captured(1))) {
            *error = QStringLiteral("capability declared twice: ") + name.captured(1);
            return {};
        }
        declarations.insert(name.captured(1), kinds.value(text));
    }
    *error = QStringLiteral("StationCapabilities::toUpdates body is unterminated");
    return {};
}

// Bounded source grammar for media shape guards. Comments and literal contents
// remain single tokens, so braces/keys inside them cannot alter function scope.
QStringList mediaTokens(const QString& source)
{
    static const QRegularExpression token(QString::fromLatin1(
        R"re(//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"|[0-9][A-Za-z0-9_'.]*|'(?:\\.|[^'\\\n])*'|[A-Za-z_][A-Za-z0-9_]*|::|[^\s])re"));
    QStringList result;
    QRegularExpressionMatchIterator it = token.globalMatch(source);
    while (it.hasNext()) {
        const QString value = it.next().captured();
        if (!value.startsWith(QStringLiteral("//")) && !value.startsWith(QStringLiteral("/*"))) {
            result.append(value);
        }
    }
    return result;
}

qsizetype mediaAnchor(const QStringList& tokens, const QStringList& anchor, QString* error)
{
    qsizetype found = -1;
    for (qsizetype i = 0; i + anchor.size() <= tokens.size(); ++i) {
        if (tokens.mid(i, anchor.size()) == anchor) {
            if (found >= 0) {
                *error = QStringLiteral("ambiguous media source anchor: ") + anchor.join(QLatin1Char(' '));
                return -1;
            }
            found = i;
        }
    }
    if (found < 0) {
        *error = QStringLiteral("missing media source anchor: ") + anchor.join(QLatin1Char(' '));
    }
    return found;
}

QStringList mediaBraces(const QStringList& tokens, qsizetype open, QString* error)
{
    if (open < 0 || open >= tokens.size() || tokens.at(open) != QStringLiteral("{")) {
        *error = QStringLiteral("media source anchor has no brace block");
        return {};
    }
    int depth = 1;
    for (qsizetype i = open + 1; i < tokens.size(); ++i) {
        if (tokens.at(i) == QStringLiteral("{")) { ++depth; }
        if (tokens.at(i) == QStringLiteral("}") && --depth == 0) {
            return tokens.mid(open + 1, i - open - 1);
        }
    }
    *error = QStringLiteral("unterminated media source brace block");
    return {};
}

QStringList mediaFunction(const QString& source, const QString& name, QString* error)
{
    const QStringList tokens = mediaTokens(source);
    QStringList anchor = mediaTokens(name);
    anchor.append(QStringLiteral("("));
    const qsizetype at = mediaAnchor(tokens, anchor, error);
    if (at < 0) { return {}; }
    int depth = 1;
    qsizetype i = at + anchor.size();
    for (; i < tokens.size() && depth > 0; ++i) {
        if (tokens.at(i) == QStringLiteral("(")) { ++depth; }
        if (tokens.at(i) == QStringLiteral(")")) { --depth; }
    }
    if (i < tokens.size() && tokens.at(i) == QStringLiteral("const")) { ++i; }
    const QStringList body = mediaBraces(tokens, i, error);
    for (qsizetype j = 0; j + 1 < body.size(); ++j) {
        if (body.at(j) == QStringLiteral("R") && body.at(j + 1).startsWith(QLatin1Char('"'))) {
            *error = QStringLiteral("raw strings are outside the bounded media source grammar");
            return {};
        }
    }
    return body;
}

QString mediaLiteral(const QString& token)
{
    static const QRegularExpression name(QString::fromLatin1(R"re(^"([A-Za-z][A-Za-z0-9-]*)"$)re"));
    const QRegularExpressionMatch match = name.match(token);
    return match.hasMatch() ? match.captured(1) : QString();
}

// exactKeys/objectWithKeys use bare string-literal lists. Refuse any other
// list syntax rather than silently recording a partial key inventory.
QSet<QStringList> mediaLiteralLists(const QStringList& tokens, const QString& call, QString* error)
{
    QSet<QStringList> result;
    for (qsizetype i = 0; i + 1 < tokens.size(); ++i) {
        if (tokens.at(i) != call || tokens.at(i + 1) != QStringLiteral("(")) { continue; }
        qsizetype brace = i + 2;
        int argumentDepth = 1;
        for (; brace < tokens.size(); ++brace) {
            if (tokens.at(brace) == QStringLiteral("(")) { ++argumentDepth; }
            if (tokens.at(brace) == QStringLiteral(")")) { --argumentDepth; }
            if (argumentDepth == 0) { break; }
            if (argumentDepth == 1 && tokens.at(brace) == QStringLiteral(",")) { ++brace; break; }
        }
        if (argumentDepth != 1 || brace >= tokens.size() || tokens.at(brace) != QStringLiteral("{")) {
            *error = QStringLiteral("unsupported second argument in ") + call;
            return {};
        }
        const QStringList values = mediaBraces(tokens, brace, error);
        if (!error->isEmpty()) { return {}; }
        QStringList keys;
        for (qsizetype j = 0; j < values.size(); ++j) {
            if (j % 2 == 1 && values.at(j) == QStringLiteral(",")) { continue; }
            const QString key = mediaLiteral(values.at(j));
            if (j % 2 != 0 || key.isEmpty() || keys.contains(key)) {
                *error = QStringLiteral("unsupported literal key-list syntax in ") + call;
                return {};
            }
            keys.append(key);
        }
        if (keys.isEmpty()) { *error = QStringLiteral("empty media key list"); return {}; }
        keys.sort();
        result.insert(keys);
    }
    if (result.isEmpty()) { *error = QStringLiteral("no key lists in ") + call; }
    return result;
}

QSet<QString> mediaMemberKeys(const QStringList& tokens, const QString& object,
                              const QString& method, QString* error, bool dynamicExtras = false)
{
    const QStringList anchor{object, QStringLiteral("."), method, QStringLiteral("(")};
    QSet<QString> keys;
    for (qsizetype i = 0; i + anchor.size() <= tokens.size(); ++i) {
        if (tokens.mid(i, anchor.size()) != anchor) { continue; }
        const qsizetype argument = i + anchor.size();
        if (dynamicExtras && tokens.mid(argument, 2)
                == (QStringList{QStringLiteral("key"), QStringLiteral(")")})) {
            keys.unite(QSet<QString>(displayExtrasSubscribeKeys().cbegin(), displayExtrasSubscribeKeys().cend()));
            continue;
        }
        if (tokens.mid(argument, 2) != (QStringList{QStringLiteral("QStringLiteral"), QStringLiteral("(")})
            || argument + 4 >= tokens.size()) {
            *error = QStringLiteral("unsupported media member-key syntax: ") + object + QLatin1Char('.') + method;
            return {};
        }
        const QString key = mediaLiteral(tokens.at(argument + 2));
        if (key.isEmpty() || tokens.at(argument + 3) != QStringLiteral(")")
            || tokens.at(argument + 4) != (method == QStringLiteral("insert")
                                             ? QStringLiteral(",") : QStringLiteral(")"))) {
            *error = QStringLiteral("nonliteral media member key"); return {};
        }
        keys.insert(key);
    }
    return keys;
}

struct MediaSourceShape {
    QSet<QString> fields;
    QSet<QString> conditional;
};

MediaSourceShape mediaRequestShape(const QString& source, const QString& function, QString* error)
{
    const QStringList body = mediaFunction(source, function, error);
    if (!error->isEmpty()) { return {}; }
    if (mediaAnchor(body, mediaTokens(QStringLiteral("exactKeys ( legacyShape , {")), error) < 0) { return {}; }
    const QSet<QStringList> bases = mediaLiteralLists(body, QStringLiteral("exactKeys"), error);
    if (!error->isEmpty() || bases.size() != 1) {
        if (error->isEmpty()) { *error = QStringLiteral("request must have one exact base shape"); }
        return {};
    }
    const QStringList base = *bases.cbegin();
    const bool extras = function.endsWith(QStringLiteral("handleSubscribe"));
    if (extras && mediaAnchor(body, mediaTokens(QStringLiteral("displayExtrasSubscribeKeys ( )")), error) < 0) {
        return {};
    }
    MediaSourceShape shape;
    shape.fields = QSet<QString>(base.cbegin(), base.cend());
    shape.conditional = mediaMemberKeys(body, QStringLiteral("legacyShape"), QStringLiteral("remove"), error, extras);
    if (!shape.fields.contains(QStringLiteral("op")) || !shape.fields.contains(QStringLiteral("connectionId"))
        || shape.conditional.isEmpty() || !(shape.fields & shape.conditional).isEmpty()) {
        *error = QStringLiteral("invalid/empty media request source inventory");
    }
    return shape;
}

QStringList mediaShapeDrift(const QString& op, const MediaSourceShape& source, const QJsonObject& recorded)
{
    const QSet<QString> fields = stringSet(recorded.value(QStringLiteral("fields")).toArray());
    const QStringList conditionalKeys = recorded.value(QStringLiteral("conditional")).toObject().keys();
    const QSet<QString> conditional(conditionalKeys.cbegin(), conditionalKeys.cend());
    QStringList drift;
    for (const QString& key : source.fields - fields) { drift.append(op + QStringLiteral(".fields missing ") + key); }
    for (const QString& key : fields - source.fields) { drift.append(op + QStringLiteral(".fields unexpected ") + key); }
    for (const QString& key : source.conditional - conditional) { drift.append(op + QStringLiteral(".conditional missing ") + key); }
    for (const QString& key : conditional - source.conditional) { drift.append(op + QStringLiteral(".conditional unexpected ") + key); }
    drift.sort();
    return drift;
}

QJsonObject mediaRecordedShape(const MediaSourceShape& shape)
{
    QJsonObject conditional;
    for (const QString& key : shape.conditional) { conditional.insert(key, QJsonArray{}); }
    return {{QStringLiteral("fields"), QJsonArray::fromStringList(QStringList(shape.fields.cbegin(), shape.fields.cend()))},
            {QStringLiteral("conditional"), conditional}};
}

QSet<QString> mediaPayloadKeys(const QStringList& tokens, QString* error)
{
    QSet<QString> keys;
    int depth = 0;
    for (qsizetype i = 0; i < tokens.size(); ++i) {
        if (tokens.at(i) == QStringLiteral("{")) {
            ++depth;
            if (depth == 1) {
                const QStringList literal = tokens.mid(i + 1, 5);
                if (literal.size() != 5 || literal.at(0) != QStringLiteral("QStringLiteral")
                    || literal.at(1) != QStringLiteral("(") || mediaLiteral(literal.at(2)).isEmpty()
                    || literal.at(3) != QStringLiteral(")") || literal.at(4) != QStringLiteral(",")) {
                    *error = QStringLiteral("unsupported media object-key syntax"); return {};
                }
                const QString key = mediaLiteral(literal.at(2));
                if (keys.contains(key)) { *error = QStringLiteral("duplicate media object key"); return {}; }
                keys.insert(key);
            }
        }
        if (tokens.at(i) == QStringLiteral("}")) { --depth; }
    }
    if (keys.isEmpty() || depth != 0) { *error = QStringLiteral("empty/unbalanced media payload keys"); }
    return keys;
}

QSet<QString> mediaInlineReply(const QString& source, const QString& function, const QString& op, QString* error)
{
    const QStringList body = mediaFunction(source, function, error);
    if (!error->isEmpty()) { return {}; }
    const qsizetype at = mediaAnchor(body, mediaTokens(QStringLiteral("sendControl ( {")), error);
    if (at < 0) { return {}; }
    const QStringList payload = mediaBraces(body, at + 2, error);
    if (!error->isEmpty()) { return {}; }
    const QStringList operation = mediaTokens(QStringLiteral("{ QStringLiteral ( \"op\" ), QStringLiteral ( \"%1\" ) }").arg(op));
    if (mediaAnchor(payload, operation, error) < 0) { return {}; }
    return mediaPayloadKeys(payload, error);
}

QSet<QStringList> mediaNestedVariants(const QString& source, const QString& field, QString* error)
{
    const QStringList body = mediaFunction(source, QStringLiteral("parseDisplayExtrasRequest"), error);
    if (!error->isEmpty()) { return {}; }
    const QStringList anchor = mediaTokens(QStringLiteral("if ( subscribe . contains ( QStringLiteral ( \"%1\" ) ) ) {").arg(field));
    const qsizetype at = mediaAnchor(body, anchor, error);
    if (at < 0) { return {}; }
    return mediaLiteralLists(mediaBraces(body, at + anchor.size() - 1, error), QStringLiteral("objectWithKeys"), error);
}

QSet<QStringList> mediaRecordedVariants(const QJsonObject& op, const QString& field)
{
    QSet<QStringList> variants;
    for (const QJsonValue& variant : op.value(QStringLiteral("nested")).toObject().value(field).toArray()) {
        QStringList keys;
        for (const QJsonValue& key : variant.toArray()) { keys.append(key.toString()); }
        keys.sort();
        variants.insert(keys);
    }
    return variants;
}

QStringList mediaVariantDrift(const QString& field, const QSet<QStringList>& source, const QSet<QStringList>& recorded)
{
    QStringList drift;
    for (const QStringList& keys : source - recorded) { drift.append(field + QStringLiteral(" missing variant ") + keys.join(QLatin1Char(','))); }
    for (const QStringList& keys : recorded - source) { drift.append(field + QStringLiteral(" unexpected variant ") + keys.join(QLatin1Char(','))); }
    drift.sort();
    return drift;
}

// Check an actual codec-produced shape against one operation's recorded
// base/conditional inventory. A same-shaped different op is still refused.
QStringList mediaCodecDrift(const QString& op, const QJsonObject& payload, const QJsonObject& recorded)
{
    QStringList drift;
    if (payload.value(QStringLiteral("op")).toString() != op) { drift.append(op + QStringLiteral(" wrong operation")); }
    const QSet<QString> base = stringSet(recorded.value(QStringLiteral("fields")).toArray());
    const QStringList optional = recorded.value(QStringLiteral("conditional")).toObject().keys();
    const QSet<QString> allowed = base + QSet<QString>(optional.cbegin(), optional.cend());
    const QStringList payloadKeys = payload.keys();
    const QSet<QString> actual(payloadKeys.cbegin(), payloadKeys.cend());
    for (const QString& key : actual - allowed) { drift.append(op + QStringLiteral(" unrecorded key ") + key); }
    for (const QString& key : base - actual) { drift.append(op + QStringLiteral(" missing base key ") + key); }
    drift.sort();
    return drift;
}

SpectrumContextMessage validMediaSpectrum()
{
    // Valid codec inputs: tst_remote_spectrum_context.cpp:44-65, in the
    // actual producer's types; no endpoint/transport/WDSP is started here.
    SpectrumContextMessage m;
    m.connectionId = QStringLiteral("00000000-0000-0000-0000-000000000001");
    m.endpointId = 7; m.revision = 3; m.contextGeneration = 42; m.sourceStream = 2;
    m.sourceCentreHz = 14225000.0; m.sampleRateHz = 192000.0;
    m.centreHz = 14225023.4375; m.spanHz = 24046.875;
    m.wideCentreHz = 14225000.0; m.wideSpanHz = 96000.0;
    m.traceSamples = 257; m.waterfallSamples = 257; m.wideSamples = 512;
    m.minDbm = -163.25; m.maxDbm = -17.5; m.fps = 30; m.framesPerLine = 2;
    m.grant = SpectrumContextGrant{4096, FftTier::Wide, 257, 257, SpectrumLimitReason::None};
    return m;
}

struct MediaSubscribeRules {
    QSet<QString> txPair;
    int duplexVersion = 0;
};

MediaSubscribeRules mediaSubscribeRules(const QString& source, QString* error)
{
    const QStringList body = mediaFunction(source, QStringLiteral("DaemonMediaController::handleSubscribe"), error);
    if (!error->isEmpty()) { return {}; }
    const QStringList anchor = mediaTokens(QStringLiteral("const bool txWindowPresent ="));
    const qsizetype at = mediaAnchor(body, anchor, error);
    if (at < 0) { return {}; }
    qsizetype end = at + anchor.size();
    while (end < body.size() && body.at(end) != QStringLiteral(";")) { ++end; }
    if (end == body.size()) { *error = QStringLiteral("unterminated TX-pair declaration"); return {}; }
    MediaSubscribeRules rules;
    rules.txPair = mediaMemberKeys(body.mid(at + anchor.size(), end - at - anchor.size()),
                                   QStringLiteral("control"), QStringLiteral("contains"), error);
    if (!error->isEmpty() || rules.txPair.size() != 2) {
        *error = QStringLiteral("subscribe TX window must discover exactly two source members"); return {};
    }
    for (const QString& key : rules.txPair) {
        if (mediaAnchor(body, mediaTokens(QStringLiteral("! control . contains ( QStringLiteral ( \"%1\" ) )").arg(key)), error) < 0) {
            return {};
        }
    }
    const QString text = body.join(QLatin1Char(' '));
    const QRegularExpression threshold(QStringLiteral(R"re(m_txDisplayDeclared < (\d+))re"));
    QRegularExpressionMatchIterator limits = threshold.globalMatch(text);
    if (!limits.hasNext()) { *error = QStringLiteral("missing duplex version source guard"); return {}; }
    rules.duplexVersion = limits.next().captured(1).toInt();
    if (limits.hasNext() || rules.duplexVersion <= 0
        || !text.contains(QStringLiteral("! m_txDisplayNegotiated"))
        || !text.contains(QStringLiteral("control . value ( QStringLiteral ( \"duplex\" ) ) . isBool ( )"))) {
        *error = QStringLiteral("unsupported duplex source guard"); return {};
    }
    return rules;
}

// Source-derived shape gate only; handler execution/session policy is outside
// this test. It models the discovered pair and negotiated-version predicates.
bool mediaSubscribeCombination(const MediaSubscribeRules& rules, const QSet<QString>& present,
                                int txVersion, const QJsonValue& duplex = QJsonValue())
{
    const QSet<QString> pair = present & rules.txPair;
    if (!pair.isEmpty() && (pair != rules.txPair || txVersion < 1)) { return false; }
    return !present.contains(QStringLiteral("duplex"))
        || (txVersion >= rules.duplexVersion && duplex.isBool());
}

QStringList mediaAudioRefusalTagDrift(const QJsonObject& payload, const QJsonObject& recorded)
{
    if (payload.contains(QStringLiteral("opusBitrateRefusal"))
        && payload.value(QStringLiteral("profile")) == QStringLiteral("lossless")
        && stringSet(recorded.value(QStringLiteral("conditional")).toObject()
                         .value(QStringLiteral("opusBitrateRefusal")).toArray()).contains(QStringLiteral("profile=opus"))) {
        return {QStringLiteral("audio-context.opusBitrateRefusal incorrectly requires profile=opus (legal lossless enabled/disabled codec shapes)")};
    }
    return {};
}

// The dispatcher's three words for a verb it does not know: dispatch()'s
// own fallthrough (and handleNotchAction's), handlePureSignalAction's, and
// DspAssetService::execute's.
bool isUnknownVerbRefusal(const SessionMessage& result)
{
    return !result.accepted
        && (result.reason
                == QStringLiteral("The Core does not know this request. Updating the Core may help.")
            || result.reason == QStringLiteral("The Core does not know this PureSignal action.")
            || result.reason == QStringLiteral("The Core does not know this request."));
}

MirrorUpdate defaultArgument(const CommandArgumentSpec& spec)
{
    switch (spec.kind) {
    case MirrorWireKind::Bool: return {0, spec.name, spec.kind, QVariant(false)};
    case MirrorWireKind::Int64:
    case MirrorWireKind::Enum: return {0, spec.name, spec.kind, QVariant(qlonglong(0))};
    case MirrorWireKind::Float64: return {0, spec.name, spec.kind, QVariant(0.0)};
    case MirrorWireKind::Utf8: return {0, spec.name, spec.kind, QVariant(QString())};
    case MirrorWireKind::Unsupported: break;
    }
    return {0, spec.name, spec.kind, QVariant()};
}


// The keys SessionMessages::encode can write for each kind, read from its
// source (`encodeSource` is SessionMessages.cpp): every
// o.insert(QStringLiteral("<key>"), ...) in the switch case(s) of that kind,
// conditional ones included, plus the keys written before the switch for
// every kind. An insert inside an if whose condition names kinds
// (message.kind == SessionMessageKind::X) counts only for those kinds.
// Kinds are keyed by their wire names, from kKindNames in the same source.
QHash<QString, QSet<QString>> encoderKeys(const QString& encodeSource, QString* error)
{
    QHash<QString, QString> wireNameOf;
    static const QRegularExpression kindName(
        QString::fromLatin1(R"re(\{\s*SessionMessageKind::(\w+),\s*"([^"]+)"\s*\})re"));
    QRegularExpressionMatchIterator names = kindName.globalMatch(encodeSource);
    while (names.hasNext()) {
        const QRegularExpressionMatch m = names.next();
        wireNameOf.insert(m.captured(1), m.captured(2));
    }
    const qsizetype begin =
        encodeSource.indexOf(QStringLiteral("QByteArray SessionMessages::encode("));
    const qsizetype end = begin < 0 ? -1
        : encodeSource.indexOf(QStringLiteral("const QByteArray wire ="), begin);
    if (wireNameOf.isEmpty() || begin < 0 || end < 0) {
        *error = QStringLiteral("SessionMessages::encode or kKindNames not found");
        return {};
    }
    static const QRegularExpression caseLabel(
        QString::fromLatin1(R"re(^\s*case\s+SessionMessageKind::(\w+)\s*:)re"));
    static const QRegularExpression insert(
        QString::fromLatin1(R"re(\bo\.insert\(\s*QStringLiteral\("([^"]+)"\))re"));
    static const QRegularExpression kindTest(
        QString::fromLatin1(R"re(message\.kind\s*==\s*SessionMessageKind::(\w+))re"));
    QHash<QString, QSet<QString>> keys;
    QSet<QString> everyKind;
    QStringList group;          // The kinds of the case being read.
    bool previousWasCase = false;
    bool inSwitch = false;
    int depth = 0;
    struct Restriction {
        int depth;
        QStringList kinds;
    };
    QList<Restriction> restrictions;
    QString pendingCondition;   // An if condition still being read.
    const QStringList lines = encodeSource.mid(begin, end - begin).split(QLatin1Char('\n'));
    for (const QString& line : lines) {
        if (line.contains(QStringLiteral("switch (message.kind)"))) {
            inSwitch = true;
        }
        const QRegularExpressionMatch label = caseLabel.match(line);
        if (inSwitch && label.hasMatch()) {
            if (!previousWasCase) {
                group.clear();
            }
            group.append(wireNameOf.value(label.captured(1)));
            previousWasCase = true;
        } else if (!line.trimmed().isEmpty()) {
            previousWasCase = false;
        }
        if (line.contains(QStringLiteral("if (")) || !pendingCondition.isEmpty()) {
            pendingCondition += line;
            if (line.contains(QLatin1Char('{'))) {
                QStringList kinds;
                QRegularExpressionMatchIterator it = kindTest.globalMatch(pendingCondition);
                while (it.hasNext()) {
                    kinds.append(wireNameOf.value(it.next().captured(1)));
                }
                if (!kinds.isEmpty()) {
                    restrictions.append({depth + 1, kinds});
                }
                pendingCondition.clear();
            } else if (line.trimmed().endsWith(QLatin1Char(';'))) {
                pendingCondition.clear();
            }
        }
        QRegularExpressionMatchIterator inserts = insert.globalMatch(line);
        while (inserts.hasNext()) {
            const QString key = inserts.next().captured(1);
            if (!inSwitch) {
                everyKind.insert(key);
                continue;
            }
            QStringList kinds = group;
            for (const Restriction& restriction : std::as_const(restrictions)) {
                QStringList narrowed;
                for (const QString& kind : std::as_const(kinds)) {
                    if (restriction.kinds.contains(kind)) {
                        narrowed.append(kind);
                    }
                }
                kinds = narrowed;
            }
            for (const QString& kind : std::as_const(kinds)) {
                keys[kind].insert(key);
            }
        }
        depth += int(line.count(QLatin1Char('{'))) - int(line.count(QLatin1Char('}')));
        while (!restrictions.isEmpty() && depth < restrictions.last().depth) {
            restrictions.removeLast();
        }
    }
    for (const QString& kind : wireNameOf) {
        keys[kind].unite(everyKind);
    }
    return keys;
}

// Where the keys surface.json records for each kind (required and
// optional) differ from what the encoder can write; empty when they agree.
QStringList encoderKeyDrift(const QString& encodeSource, const QJsonObject& messageKinds)
{
    QString error;
    const QHash<QString, QSet<QString>> written = encoderKeys(encodeSource, &error);
    if (!error.isEmpty()) {
        return {error};
    }
    QStringList drift;
    for (auto it = messageKinds.constBegin(); it != messageKinds.constEnd(); ++it) {
        const QJsonObject entry = it.value().toObject();
        QSet<QString> recorded = stringSet(entry.value(QStringLiteral("required")).toArray());
        recorded.unite(stringSet(entry.value(QStringLiteral("optional")).toArray()));
        const QSet<QString> encoder = written.value(it.key());
        for (const QString& key : encoder - recorded) {
            drift.append(QStringLiteral("%1: the encoder can write \"%2\", which surface.json "
                                        "does not record")
                             .arg(it.key(), key));
        }
        for (const QString& key : recorded - encoder) {
            drift.append(QStringLiteral("%1: surface.json records \"%2\", which the encoder "
                                        "never writes")
                             .arg(it.key(), key));
        }
    }
    drift.sort();
    return drift;
}
} // namespace

class TstLinkSurfaceManifest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void surfaceHasExactlyTheNineSections();
    void captureRecordsNoErrors();
    void capabilitiesIncludeEveryDeclaredNameAndKind();
    void committedSurfaceMatchesTheCode();
    void driftGuardNamesEachKindOfChange();
    void messageKeysAreWhatTheEncoderWrites();
    void aKeyTheSampleNeverSetsIsStillCaught();

    void allKindsListsEveryEnumeratorInOrder();
    void mirroredClassesAreTheSchemaAllowlist();
    void verbTableMatchesTheRouting();
    void verbCapabilitiesAreAdvertised();
    void everyListedVerbIsRoutedAndNoOtherIs();
    void mediaOperationsMatchBothEnds();
    void mediaRequestFieldsAreOperationScoped();
    void mediaConditionalPairAndVersionStayScoped();
    void mediaSpectrumTransmitUsesActualCodec();
    void mediaNestedLegacyShapesUseActualParser();
    void mediaAudioBitrateRefusalIncludesLossless();
    void mediaInlineAndMonitorRepliesAreExact();
    void mediaSourceDriftNegativeControls();
    void limitsMatchTheirSources();

private:
    QJsonObject captured();
    QJsonObject m_captured;
};

void TstLinkSurfaceManifest::initTestCase()
{
    // RadioModel reads AppSettings::instance(); keep this process's copy
    // private, as tst_station_session does.
    const QString profile =
        QStringLiteral("link-surface-%1").arg(QCoreApplication::applicationPid());
    AppSettings::setProfileOverride(profile);
    QCOMPARE(AppSettings::instance().filePath(), AppSettings::resolveSettingsPath(profile));
    AppSettings::instance().clear();
}

void TstLinkSurfaceManifest::cleanupTestCase()
{
    const QString path = AppSettings::instance().filePath();
    QFile::remove(path);
    QFile::remove(path + QStringLiteral(".bak"));
}

QJsonObject TstLinkSurfaceManifest::captured()
{
    if (m_captured.isEmpty()) {
        m_captured = LinkSurface::capture();
    }
    return m_captured;
}

void TstLinkSurfaceManifest::surfaceHasExactlyTheNineSections()
{
    QStringList expected = LinkSurface::sectionNames();
    QCOMPARE(expected.size(), 9);
    QStringList keys = captured().keys();
    keys.sort();
    expected.sort();
    QCOMPARE(keys, expected);
}

void TstLinkSurfaceManifest::captureRecordsNoErrors()
{
    const QJsonObject surface = captured();
    const QJsonObject kinds = surface.value(QStringLiteral("messageKinds")).toObject();
    QCOMPARE(kinds.size(), SessionMessages::allKinds().size());
    for (auto it = kinds.constBegin(); it != kinds.constEnd(); ++it) {
        QVERIFY2(!it.value().toObject().contains(QStringLiteral("error")),
                 qPrintable(it.key() + QStringLiteral(": ")
                            + it.value().toObject().value(QStringLiteral("error")).toString()));
        QVERIFY2(stringSet(it.value().toObject().value(QStringLiteral("required")).toArray())
                     .contains(QStringLiteral("type")),
                 qPrintable(it.key()));
    }
    // The shape the brief gives for hello.
    QCOMPARE(stringSet(kinds.value(QStringLiteral("hello")).toObject()
                           .value(QStringLiteral("required")).toArray()),
             (QSet<QString>{QStringLiteral("type"), QStringLiteral("major"),
                            QStringLiteral("minor"), QStringLiteral("settingsSchema"),
                            QStringLiteral("peer")}));

    const QJsonArray keys = surface.value(QStringLiteral("objectKeys")).toArray();
    QVERIFY(!keys.isEmpty());
    QSet<QString> patterns;
    for (const QJsonValue& v : keys) {
        QVERIFY2(!v.toObject().contains(QStringLiteral("error")),
                 qPrintable(v.toObject().value(QStringLiteral("error")).toString()));
        patterns.insert(v.toObject().value(QStringLiteral("key")).toString());
    }
    for (const char* key : {"radio", "transmit", "tuner", "pan:<i>", "slice:<id>"}) {
        QVERIFY2(patterns.contains(QString::fromLatin1(key)), key);
    }
    // Every mirrored class reaches the wire under some key.
    QSet<QByteArray> classes;
    for (const QJsonValue& v : keys) {
        classes.insert(v.toObject().value(QStringLiteral("class")).toString().toUtf8());
    }
    const QList<QByteArray> mirrored = MirrorSchema::mirroredClassNames();
    QCOMPARE(classes, QSet<QByteArray>(mirrored.cbegin(), mirrored.cend()));
}

void TstLinkSurfaceManifest::capabilitiesIncludeEveryDeclaredNameAndKind()
{
    const QString source = readSource(QStringLiteral("src/core/session/StationCapabilities.cpp"));
    QVERIFY(!source.isEmpty());
    QString error;
    const QHash<QString, QString> declared = capabilityDeclarations(source, &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QVERIFY(!declared.isEmpty());
    QCOMPARE(declared.value(QStringLiteral("txWatchPathVersion")), QStringLiteral("i64"));

    // Decoys in comments, strings and other functions cannot become names.
    const QString probe = QString::fromLatin1(R"cpp(
QList<MirrorUpdate> StationCapabilities::toUpdates() const
{
    // intEntry("commentDecoy", 1); }
    /* stringEntry("blockDecoy", "{"); */
    const char* reason = "intEntry(\"stringDecoy\", 1); }";
    const char brace = '}';
    return {intEntry("realVersion", 1), boolEntry("realFlag", true),
            stringEntry("realText", "not a capability name")};
}
void anotherFunction() { intEntry("outsideDecoy", 1); }
)cpp");
    const QHash<QString, QString> parsedProbe = capabilityDeclarations(probe, &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(parsedProbe, (QHash<QString, QString>{{QStringLiteral("realVersion"), QStringLiteral("i64")},
                                                  {QStringLiteral("realFlag"), QStringLiteral("bool")},
                                                  {QStringLiteral("realText"), QStringLiteral("utf8")}}));

    QHash<QString, QString> recorded;
    int watchEntries = 0;
    for (const QJsonValue& value : captured().value(QStringLiteral("capabilities")).toArray()) {
        const QJsonObject entry = value.toObject();
        const QString name = entry.value(QStringLiteral("name")).toString();
        QVERIFY2(!name.isEmpty(), "captured capability has no name");
        QVERIFY2(!recorded.contains(name), qPrintable(QStringLiteral("captured twice: ") + name));
        QVERIFY2(!entry.contains(QStringLiteral("error")),
                 qPrintable(name + QStringLiteral(": ")
                            + entry.value(QStringLiteral("error")).toString()));
        recorded.insert(name, entry.value(QStringLiteral("kind")).toString());
        if (name == QStringLiteral("txWatchPathVersion")) {
            ++watchEntries;
            QCOMPARE(entry.value(QStringLiteral("kind")).toString(), QStringLiteral("i64"));
            // The LoopbackTransport fixture is not a supported watch primary.
            // A declaration is required; a live value must not be invented.
            QVERIFY2(!entry.contains(QStringLiteral("value")),
                     "unsupported watch fixture must not claim a live capability value");
        } else {
            QVERIFY2(entry.contains(QStringLiteral("value")),
                     qPrintable(QStringLiteral("missing live capability value: ") + name));
        }
    }
    const QStringList declaredKeys = declared.keys();
    const QStringList recordedKeys = recorded.keys();
    const QSet<QString> declaredNames(declaredKeys.cbegin(), declaredKeys.cend());
    const QSet<QString> recordedNames(recordedKeys.cbegin(), recordedKeys.cend());
    QVERIFY2(declaredNames == recordedNames,
             qPrintable(QStringLiteral("capability declaration drift; missing: ")
                        + joined(declaredNames - recordedNames)
                        + QStringLiteral("; unexpected: ") + joined(recordedNames - declaredNames)));
    for (const QString& name : declaredNames) {
        QVERIFY2(recorded.value(name) == declared.value(name),
                 qPrintable(QStringLiteral("capability wire kind differs: ") + name));
    }
    QCOMPARE(watchEntries, 1);
}

void TstLinkSurfaceManifest::committedSurfaceMatchesTheCode()
{
    QFile file(QStringLiteral(NEREUS_LINK_DATA_DIR "/surface.json"));
    QVERIFY2(file.open(QIODevice::ReadOnly), qPrintable(file.fileName()));
    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    QVERIFY2(error.error == QJsonParseError::NoError, qPrintable(error.errorString()));
    QVERIFY(doc.isObject());
    const QStringList diff = LinkSurface::differences(doc.object(), captured());
    QVERIFY2(diff.isEmpty(),
             qPrintable(QStringLiteral("tests/data/link/v1/surface.json no longer matches the "
                                       "code; regenerate it with "
                                       "tst_link_surface_manifest_regen and review:\n  ")
                        + diff.join(QStringLiteral("\n  "))));
}

void TstLinkSurfaceManifest::driftGuardNamesEachKindOfChange()
{
    const QJsonObject surface = captured();
    QVERIFY(LinkSurface::differences(surface, surface).isEmpty());

    // A capability added.
    {
        QJsonObject changed = surface;
        QJsonArray caps = changed.value(QStringLiteral("capabilities")).toArray();
        caps.append(QJsonObject{{QStringLiteral("name"), QStringLiteral("linkSurfaceProbeVersion")},
                                {QStringLiteral("kind"), QStringLiteral("i64")}});
        changed.insert(QStringLiteral("capabilities"), caps);
        const QStringList diff = LinkSurface::differences(surface, changed);
        QCOMPARE(diff.size(), 1);
        QVERIFY2(diff.first().startsWith(
                     QStringLiteral("capabilities[linkSurfaceProbeVersion]: added")),
                 qPrintable(diff.first()));
    }
    // A mirrored property changes kind.
    {
        QJsonObject changed = surface;
        QJsonObject classes = changed.value(QStringLiteral("mirrorClasses")).toObject();
        QJsonObject slice = classes.value(QStringLiteral("SliceModel")).toObject();
        QJsonArray properties = slice.value(QStringLiteral("properties")).toArray();
        bool found = false;
        for (qsizetype i = 0; i < properties.size(); ++i) {
            QJsonObject prop = properties.at(i).toObject();
            if (prop.value(QStringLiteral("name")).toString() == QStringLiteral("frequency")) {
                QVERIFY(prop.value(QStringLiteral("kind")).toString() != QStringLiteral("utf8"));
                prop.insert(QStringLiteral("kind"), QStringLiteral("utf8"));
                properties.replace(i, prop);
                found = true;
            }
        }
        QVERIFY(found);
        slice.insert(QStringLiteral("properties"), properties);
        classes.insert(QStringLiteral("SliceModel"), slice);
        changed.insert(QStringLiteral("mirrorClasses"), classes);
        const QStringList diff = LinkSurface::differences(surface, changed);
        QCOMPARE(diff.size(), 1);
        QVERIFY2(diff.first().startsWith(
                     QStringLiteral("mirrorClasses.SliceModel.properties[frequency].kind: ")),
                 qPrintable(diff.first()));
        QVERIFY2(diff.first().endsWith(QStringLiteral("-> \"utf8\"")), qPrintable(diff.first()));
    }
    // A verb gains an argument.
    {
        QJsonObject changed = surface;
        QJsonArray commands = changed.value(QStringLiteral("commands")).toArray();
        bool found = false;
        for (qsizetype i = 0; i < commands.size(); ++i) {
            QJsonObject command = commands.at(i).toObject();
            if (command.value(QStringLiteral("verb")).toString() == QStringLiteral("addSlice")) {
                QJsonArray arguments = command.value(QStringLiteral("arguments")).toArray();
                arguments.append(QJsonObject{{QStringLiteral("name"), QStringLiteral("probe")},
                                             {QStringLiteral("kind"), QStringLiteral("bool")},
                                             {QStringLiteral("optional"), false}});
                command.insert(QStringLiteral("arguments"), arguments);
                commands.replace(i, command);
                found = true;
            }
        }
        QVERIFY(found);
        changed.insert(QStringLiteral("commands"), commands);
        const QStringList diff = LinkSurface::differences(surface, changed);
        QCOMPARE(diff.size(), 1);
        QVERIFY2(diff.first().startsWith(
                     QStringLiteral("commands[addSlice].arguments[probe]: added")),
                 qPrintable(diff.first()));
    }
}

void TstLinkSurfaceManifest::messageKeysAreWhatTheEncoderWrites()
{
    // The capture learns a kind's keys from one sample message, so an
    // optional key its sample never sets would not be recorded. The
    // encoder's own source is the other half of the check: every key it can
    // write for a kind is recorded for that kind, and nothing else is.
    const QString source = readSource(QStringLiteral("src/core/session/SessionMessages.cpp"));
    QVERIFY(!source.isEmpty());
    const QJsonObject kinds = captured().value(QStringLiteral("messageKinds")).toObject();
    const QStringList drift = encoderKeyDrift(source, kinds);
    QVERIFY2(drift.isEmpty(), qPrintable(drift.join(QLatin1Char('\n'))));
    // And the reading itself found the conditional keys it must.
    QString error;
    const QHash<QString, QSet<QString>> keys = encoderKeys(source, &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QVERIFY(keys.value(QStringLiteral("hello")).contains(QStringLiteral("majors")));
    QVERIFY(keys.value(QStringLiteral("property.write")).contains(QStringLiteral("writeId")));
    QVERIFY(keys.value(QStringLiteral("settings.reject")).contains(QStringLiteral("reason")));
    QVERIFY(!keys.value(QStringLiteral("settings.write")).contains(QStringLiteral("reason")));
    QVERIFY(keys.value(QStringLiteral("settings.value")).contains(QStringLiteral("origin")));
    QVERIFY(!keys.value(QStringLiteral("settings.remove")).contains(QStringLiteral("origin")));
}

void TstLinkSurfaceManifest::aKeyTheSampleNeverSetsIsStillCaught()
{
    // A planted optional key, written only when a field no sample sets is
    // present: the capture would not see it; the encoder's source does.
    QString source = readSource(QStringLiteral("src/core/session/SessionMessages.cpp"));
    const QString anchor = QStringLiteral("        if (message.featuresOnWire) {");
    QVERIFY(source.contains(anchor));
    source.replace(anchor,
                   QStringLiteral("        if (!message.token.isEmpty()) {\n"
                                  "            o.insert(QStringLiteral(\"plantedKey\"), 1);\n"
                                  "        }\n")
                       + anchor);
    const QJsonObject kinds = captured().value(QStringLiteral("messageKinds")).toObject();
    const QStringList drift = encoderKeyDrift(source, kinds);
    QCOMPARE(drift, QStringList{QStringLiteral(
                        "hello: the encoder can write \"plantedKey\", which surface.json does "
                        "not record")});
}

void TstLinkSurfaceManifest::allKindsListsEveryEnumeratorInOrder()
{
    const QString header = readSource(QStringLiteral("src/core/session/SessionMessages.h"));
    QVERIFY(!header.isEmpty());
    const QRegularExpression body(QStringLiteral(
        R"re(enum class SessionMessageKind \{(.*?)\};)re"),
        QRegularExpression::DotMatchesEverythingOption);
    const QRegularExpressionMatch m = body.match(header);
    QVERIFY(m.hasMatch());
    QString text = m.captured(1);
    text.remove(QRegularExpression(QStringLiteral("//[^\n]*")));
    QStringList enumerators;
    for (const QString& part : text.split(QLatin1Char(','))) {
        const QString name = part.trimmed();
        if (!name.isEmpty()) {
            enumerators.append(name);
        }
    }
    const QList<SessionMessageKind> kinds = SessionMessages::allKinds();
    QCOMPARE(kinds.size(), enumerators.size());
    QSet<QByteArray> names;
    for (qsizetype i = 0; i < kinds.size(); ++i) {
        // No enumerator carries an explicit value, so declaration order is
        // value order.
        QCOMPARE(static_cast<int>(kinds.at(i)), static_cast<int>(i));
        const QByteArray name = SessionMessages::kindName(kinds.at(i));
        QVERIFY2(!name.isEmpty(), qPrintable(enumerators.at(i)));
        names.insert(name);
    }
    QCOMPARE(names.size(), kinds.size());
}

void TstLinkSurfaceManifest::mirroredClassesAreTheSchemaAllowlist()
{
    QList<QByteArray> names;
    for (const QMetaObject* mo : LinkSurface::mirroredMetaObjects()) {
        names.append(MirrorSchema::shortClassName(QByteArray(mo->className())));
    }
    QCOMPARE(names, MirrorSchema::mirroredClassNames());
}

void TstLinkSurfaceManifest::verbTableMatchesTheRouting()
{
    const QString dispatcher =
        readSource(QStringLiteral("src/core/session/SessionCommandDispatcher.cpp"));
    QVERIFY(!dispatcher.isEmpty());
    // Only the routing code, not the declared table: scan from dispatch().
    const qsizetype start = dispatcher.indexOf(
        QStringLiteral("void SessionCommandDispatcher::dispatch("));
    QVERIFY(start > 0);
    const QString routing = dispatcher.mid(start);

    const QSet<QString> exact =
        matches(routing, QStringLiteral(R"re(commandVerb\s*==\s*"([^"]+)")re"));
    const QSet<QString> prefixes =
        matches(routing, QStringLiteral(R"re(commandVerb\.startsWith\("([^"]+)"\))re"));
    QVERIFY(!exact.isEmpty());
    QVERIFY(!prefixes.isEmpty());

    QSet<QString> listed;
    for (const CommandVerbSpec& spec : SessionCommandDispatcher::verbSpecs()) {
        const QString verb = QString::fromUtf8(spec.verb);
        QVERIFY2(!listed.contains(verb), qPrintable(QStringLiteral("listed twice: ") + verb));
        listed.insert(verb);
    }

    // A routed verb missing from the table.
    for (const QString& verb : exact) {
        QVERIFY2(listed.contains(verb),
                 qPrintable(QStringLiteral("routed but not in verbSpecs(): ") + verb));
    }
    // A listed verb that is not routed: neither an exact literal nor under
    // a routed prefix.
    for (const QString& verb : listed) {
        bool routed = exact.contains(verb);
        for (const QString& prefix : prefixes) {
            routed = routed || verb.startsWith(prefix);
        }
        QVERIFY2(routed, qPrintable(QStringLiteral("in verbSpecs() but not routed: ") + verb));
    }

    // Each prefix family: the concrete verbs its handler accepts are the
    // ones the table lists under that prefix.
    const auto listedUnder = [&listed](const QString& prefix) {
        QSet<QString> out;
        for (const QString& verb : listed) {
            if (verb.startsWith(prefix)) {
                out.insert(verb);
            }
        }
        return out;
    };
    QCOMPARE(prefixes, (QSet<QString>{QStringLiteral("ps3."), QStringLiteral("dspAssets."),
                                      QStringLiteral("notch.")}));

    const QSet<QString> notch =
        matches(routing, QStringLiteral(R"re(\bverb\s*==\s*"(notch\.[A-Za-z]+)")re"));
    QVERIFY2(notch == listedUnder(QStringLiteral("notch.")),
             qPrintable(joined(notch) + QStringLiteral(" vs ")
                        + joined(listedUnder(QStringLiteral("notch.")))));

    QSet<QString> ps3 = matches(readSource(QStringLiteral(
                                    "src/core/session/PureSignalSessionFacade.cpp")),
                                QStringLiteral(R"re(return\s+"(ps3\.[A-Za-z]+)")re"));
    ps3.unite(matches(routing, QStringLiteral(R"re(commandVerb\s*==\s*"(ps3\.[A-Za-z]+)")re")));
    QVERIFY2(ps3 == listedUnder(QStringLiteral("ps3.")),
             qPrintable(joined(ps3) + QStringLiteral(" vs ")
                        + joined(listedUnder(QStringLiteral("ps3.")))));

    const QSet<QString> dspAssets =
        matches(readSource(QStringLiteral("src/core/dsp/DspAssetService.cpp")),
                QStringLiteral(R"re(\bverb\s*==\s*"(dspAssets\.[A-Za-z0-9]+)")re"));
    QVERIFY2(dspAssets == listedUnder(QStringLiteral("dspAssets.")),
             qPrintable(joined(dspAssets) + QStringLiteral(" vs ")
                        + joined(listedUnder(QStringLiteral("dspAssets.")))));
}

void TstLinkSurfaceManifest::verbCapabilitiesAreAdvertised()
{
    QSet<QString> capabilities;
    for (const QJsonValue& v : captured().value(QStringLiteral("capabilities")).toArray()) {
        capabilities.insert(v.toObject().value(QStringLiteral("name")).toString());
    }
    for (const CommandVerbSpec& spec : SessionCommandDispatcher::verbSpecs()) {
        for (const CommandArgumentSpec& argument : spec.arguments) {
            QVERIFY2(argument.kind != MirrorWireKind::Unsupported, spec.verb.constData());
        }
        QVERIFY2(spec.minMinor <= kSessionProtocolMinor, spec.verb.constData());
        if (spec.capability.isEmpty()) {
            QCOMPARE(spec.capabilityVersion, 0);
            continue;
        }
        QVERIFY2(capabilities.contains(QString::fromUtf8(spec.capability)),
                 spec.verb.constData());
        QVERIFY2(spec.capabilityVersion >= 1, spec.verb.constData());
    }
}

void TstLinkSurfaceManifest::everyListedVerbIsRoutedAndNoOtherIs()
{
    std::unique_ptr<ConnectableRadioModel> harness = ConnectableRadioModel::create();
    QVERIFY(harness != nullptr);
    SessionCommandDispatcher dispatcher(&harness->model());
    QHash<quint32, SessionMessage> results;
    connect(&dispatcher, &SessionCommandDispatcher::commandResultReady, this,
            [&results](const SessionMessage& result) {
        if (!results.contains(result.commandId)) {
            results.insert(result.commandId, result);
        }
    });

    const QList<CommandVerbSpec>& specs = SessionCommandDispatcher::verbSpecs();
    quint32 id = 0;
    QHash<quint32, QByteArray> verbById;
    for (const CommandVerbSpec& spec : specs) {
        QList<MirrorUpdate> arguments;
        for (const CommandArgumentSpec& argument : spec.arguments) {
            arguments.append(defaultArgument(argument));
        }
        ++id;
        verbById.insert(id, spec.verb);
        dispatcher.dispatch(SessionMessages::commandInvoke(spec.verb, id, arguments));
    }
    const QList<QByteArray> unlisted{"linkSurfaceUnlisted", "nnr.linkSurfaceUnlisted",
                                     "notch.linkSurfaceUnlisted", "ps3.linkSurfaceUnlisted",
                                     "dspAssets.linkSurfaceUnlisted"};
    for (const QByteArray& verb : unlisted) {
        ++id;
        verbById.insert(id, verb);
        dispatcher.dispatch(SessionMessages::commandInvoke(verb, id, {}));
    }

    // Only "routed or not" is asked here: with default arguments on this
    // one harness many verbs are refused for reasons of their own. That a
    // verb reads its arguments (a renamed one gets a different answer) is
    // held per verb, on a station set up for it, by the session fixtures'
    // rightAndWrongLegsGetDifferentAnswers (tst_link_conformance_session).

    // requestSliceSampleRate answers on a later turn of the model's loop.
    QTRY_COMPARE_WITH_TIMEOUT(results.size(), static_cast<int>(id), 10000);
    for (quint32 i = 1; i <= id; ++i) {
        const QByteArray verb = verbById.value(i);
        const SessionMessage result = results.value(i);
        QCOMPARE(result.commandVerb, verb);
        const bool listed = i <= static_cast<quint32>(specs.size());
        if (listed) {
            QVERIFY2(!isUnknownVerbRefusal(result),
                     qPrintable(QString::fromUtf8(verb) + QStringLiteral(": ") + result.reason));
        } else {
            QVERIFY2(isUnknownVerbRefusal(result),
                     qPrintable(QString::fromUtf8(verb) + QStringLiteral(": ") + result.reason));
        }
    }
}

void TstLinkSurfaceManifest::mediaOperationsMatchBothEnds()
{
    const QJsonObject media = captured().value(QStringLiteral("mediaControl")).toObject();
    const QJsonObject guiToCore = media.value(QStringLiteral("guiToCore")).toObject();
    const QJsonObject coreToGui = media.value(QStringLiteral("coreToGui")).toObject();

    const QString daemon =
        readSource(QStringLiteral("src/core/session/media/DaemonMediaController.cpp"));
    const QString peer = readSource(QStringLiteral("src/core/session/media/MediaPeer.cpp"));
    const QString gui = readSource(QStringLiteral("src/gui/RemoteMediaController.cpp"));
    QVERIFY(!daemon.isEmpty() && !peer.isEmpty() && !gui.isEmpty());

    // The operations each end dispatches on.
    const QSet<QString> peerOps =
        matches(peer, QStringLiteral(R"re(constexpr char k[A-Za-z]+Op\[\]\s*=\s*"([a-z-]+)")re"));
    QSet<QString> coreReceives =
        matches(daemon, QStringLiteral(R"re(op\s*==\s*QLatin1String\("([a-z-]+)"\))re"));
    coreReceives.unite(peerOps);
    const QSet<QString> guiReceives =
        matches(gui, QStringLiteral(R"re(op\s*[=!]=\s*QLatin1String\("([a-z-]+)"\))re"));
    const QStringList guiToCoreKeys = guiToCore.keys();
    const QStringList coreToGuiKeys = coreToGui.keys();
    QVERIFY2(QSet<QString>(guiToCoreKeys.cbegin(), guiToCoreKeys.cend()) == coreReceives,
             qPrintable(joined(coreReceives)));
    QVERIFY2(QSet<QString>(coreToGuiKeys.cbegin(), coreToGuiKeys.cend()) == guiReceives,
             qPrintable(joined(guiReceives)));

    // Every exact key set the Core checks on arrival is one declared shape
    // of a GUI-to-Core operation: its fields, or its fields plus one
    // conditional field.
    QSet<QSet<QString>> declaredShapes;
    for (auto it = guiToCore.constBegin(); it != guiToCore.constEnd(); ++it) {
        const QJsonObject op = it.value().toObject();
        const QSet<QString> base = stringSet(op.value(QStringLiteral("fields")).toArray());
        declaredShapes.insert(base);
        const QJsonObject conditional = op.value(QStringLiteral("conditional")).toObject();
        for (auto c = conditional.constBegin(); c != conditional.constEnd(); ++c) {
            declaredShapes.insert(base + QSet<QString>{c.key()});
        }
    }
    const QRegularExpression exactKeysCall(QStringLiteral(
        R"re((?:hasExactKeys|exactKeys)\(\s*\w+\s*,\s*\{([^}]*)\})re"));
    int checked = 0;
    for (const QString& source : {daemon, peer}) {
        auto it = exactKeysCall.globalMatch(source);
        while (it.hasNext()) {
            const QSet<QString> keys = matches(it.next().captured(1),
                                               QStringLiteral(R"re("([A-Za-z0-9]+)")re"));
            if (!keys.contains(QStringLiteral("op"))) {
                continue; // a nested object's keys (parsePlane)
            }
            ++checked;
            QVERIFY2(declaredShapes.contains(keys),
                     qPrintable(QStringLiteral("the Core checks an undeclared shape: ")
                                + joined(keys)));
        }
    }
    QVERIFY(checked >= 10);

    // Every Core-to-GUI operation built inline by the Core carries exactly
    // its declared fields.
    const auto builtKeys = [](const QString& source, const QString& anchor) {
        const qsizetype at = source.indexOf(anchor);
        if (at < 0) {
            return QSet<QString>();
        }
        const QRegularExpression end(QStringLiteral(R"re(\}\)?;)re"));
        const QRegularExpressionMatch stop = end.match(source, at);
        const QString body = source.mid(at, stop.capturedStart() - at);
        QSet<QString> keys = matches(body, QStringLiteral(R"re(\{QStringLiteral\("([A-Za-z0-9]+)"\),)re"));
        keys.insert(QStringLiteral("op"));
        return keys;
    };
    const QList<QPair<QString, QString>> built{
        {QStringLiteral("rejected"), daemon},
        {QStringLiteral("allocation-result"), daemon},
        {QStringLiteral("noise-floor"), daemon},
        {QStringLiteral("clock-echo"), daemon},
    };
    for (const auto& [op, source] : built) {
        const QSet<QString> keys = builtKeys(
            source, QStringLiteral("{QStringLiteral(\"op\"), QStringLiteral(\"%1\")}").arg(op));
        QVERIFY2(!keys.isEmpty(), qPrintable(op));
        QCOMPARE(keys, stringSet(coreToGui.value(op).toObject()
                                     .value(QStringLiteral("fields")).toArray()));
    }
    for (const auto& [op, constant] : {qMakePair(QStringLiteral("description"),
                                                 QStringLiteral("kDescriptionOp")),
                                       qMakePair(QStringLiteral("candidate"),
                                                 QStringLiteral("kCandidateOp"))}) {
        const QSet<QString> keys = builtKeys(
            peer, QStringLiteral("{QStringLiteral(\"op\"), QLatin1String(%1)}").arg(constant));
        QVERIFY2(!keys.isEmpty(), qPrintable(op));
        QCOMPARE(keys, stringSet(coreToGui.value(op).toObject()
                                     .value(QStringLiteral("fields")).toArray()));
        QCOMPARE(keys, stringSet(guiToCore.value(op).toObject()
                                     .value(QStringLiteral("fields")).toArray()));
    }
}

void TstLinkSurfaceManifest::mediaRequestFieldsAreOperationScoped()
{
    const QString core = readSource(QStringLiteral("src/core/session/media/DaemonMediaController.cpp"));
    const QString gui = readSource(QStringLiteral("src/gui/RemoteMediaController.cpp"));
    QVERIFY(!core.isEmpty() && !gui.isEmpty());
    const QJsonObject requests = captured().value(QStringLiteral("mediaControl")).toObject()
                                     .value(QStringLiteral("guiToCore")).toObject();
    QStringList drift;
    QString error;
    const MediaSourceShape start = mediaRequestShape(core, QStringLiteral("DaemonMediaController::handleStart"), &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    const MediaSourceShape subscribe = mediaRequestShape(core, QStringLiteral("DaemonMediaController::handleSubscribe"), &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    const QStringList producer = mediaFunction(gui, QStringLiteral("RemoteMediaController::start"), &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    const QSet<QString> inserted = mediaMemberKeys(producer, QStringLiteral("startControl"), QStringLiteral("insert"), &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(inserted, start.conditional);
    const qsizetype object = mediaAnchor(producer, mediaTokens(QStringLiteral("QJsonObject startControl {")), &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    const QSet<QString> producerBase = mediaPayloadKeys(mediaBraces(producer, object + 2, &error), &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    const QStringList send = mediaFunction(gui, QStringLiteral("RemoteMediaController::send"), &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    const QSet<QString> transportKeys = mediaMemberKeys(send, QStringLiteral("payload"), QStringLiteral("insert"), &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(producerBase + transportKeys, start.fields);
    QVERIFY2(mediaAnchor(producer, mediaTokens(QStringLiteral("{ QStringLiteral ( \"op\" ), QStringLiteral ( \"start\" ) }")), &error) >= 0,
             qPrintable(error));
    for (const QString& function : {QStringLiteral("QJsonObject requestFor"), QStringLiteral("QJsonObject requestMini")}) {
        const QStringList builder = mediaFunction(gui, function, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        const QSet<QString> producerOptionals = mediaMemberKeys(builder, QStringLiteral("request"), QStringLiteral("insert"), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY2((producerOptionals - subscribe.conditional).isEmpty(), qPrintable(joined(producerOptionals - subscribe.conditional)));
        // Both normal and mini producers put the TX pair in one if block.
        const QString condition = function.endsWith(QStringLiteral("requestFor"))
            ? QStringLiteral("if ( inputs . txDbmWindow ) {")
            : QStringLiteral("if ( transmit & & source & & txDisplayNegotiated ) {");
        const QStringList group = mediaTokens(condition);
        const qsizetype pairAt = mediaAnchor(builder, group, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        const QSet<QString> pairKeys = mediaMemberKeys(mediaBraces(builder, pairAt + group.size() - 1, &error),
                                                      QStringLiteral("request"), QStringLiteral("insert"), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        const MediaSubscribeRules rules = mediaSubscribeRules(core, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(pairKeys, rules.txPair);
        if (function.endsWith(QStringLiteral("requestFor"))) {
            const QStringList duplexAnchor = mediaTokens(QStringLiteral("if ( inputs . duplex ) {"));
            const qsizetype duplexAt = mediaAnchor(builder, duplexAnchor, &error);
            QVERIFY2(error.isEmpty(), qPrintable(error));
            const QSet<QString> duplexKeys = mediaMemberKeys(mediaBraces(builder, duplexAt + duplexAnchor.size() - 1, &error),
                                                            QStringLiteral("request"), QStringLiteral("insert"), &error);
            QVERIFY2(error.isEmpty(), qPrintable(error));
            QCOMPARE(duplexKeys, QSet<QString>{QStringLiteral("duplex")});
        }
    }
    drift.append(mediaShapeDrift(QStringLiteral("start"), start, requests.value(QStringLiteral("start")).toObject()));
    drift.append(mediaShapeDrift(QStringLiteral("subscribe"), subscribe, requests.value(QStringLiteral("subscribe")).toObject()));
    // Independent start declarations may co-occur: check every subset
    // against the source-derived inventory, rather than only base+one.
    const QStringList declarations(inserted.cbegin(), inserted.cend());
    QVERIFY(declarations.size() < 16);
    const QJsonObject declaredStart = mediaRecordedShape(start);
    for (quint32 mask = 0; mask < (quint32(1) << declarations.size()); ++mask) {
        QJsonObject payload{{QStringLiteral("op"), QStringLiteral("start")},
                            {QStringLiteral("connectionId"), QStringLiteral("fixture")}};
        for (qsizetype i = 0; i < declarations.size(); ++i) {
            if (mask & (quint32(1) << i)) { payload.insert(declarations.at(i), 1); }
        }
        QVERIFY(mediaCodecDrift(QStringLiteral("start"), payload, declaredStart).isEmpty());
    }
    QVERIFY2(drift.isEmpty(), qPrintable(drift.join(QLatin1Char('\n'))));
}

void TstLinkSurfaceManifest::mediaConditionalPairAndVersionStayScoped()
{
    const QString source = readSource(QStringLiteral("src/core/session/media/DaemonMediaController.cpp"));
    QString error;
    const QStringList body = mediaFunction(source, QStringLiteral("DaemonMediaController::handleSubscribe"), &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    const QString normalized = body.join(QLatin1Char(' '));
    // The exact source guard proves both edges, rather than declaring two
    // individually legal singleton shapes in the manifest.
    for (const QString& key : {QStringLiteral("txMinDbm"), QStringLiteral("txMaxDbm")}) {
        QVERIFY2(mediaAnchor(body, mediaTokens(QStringLiteral("! control . contains ( QStringLiteral ( \"%1\" ) )").arg(key)), &error) >= 0,
                 qPrintable(error));
    }
    const QRegularExpression threshold(QStringLiteral(R"re(m_txDisplayDeclared < (\d+))re"));
    const QRegularExpressionMatch version = threshold.match(normalized);
    QVERIFY(version.hasMatch());
    QCOMPARE(version.captured(1).toInt(), 3);
    QVERIFY(normalized.contains(QStringLiteral("! m_txDisplayNegotiated")));
    QVERIFY(normalized.contains(QStringLiteral("control . value ( QStringLiteral ( \"duplex\" ) ) . isBool ( )")));
    const MediaSubscribeRules rules = mediaSubscribeRules(source, &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(rules.duplexVersion, 3);
    const QStringList edges(rules.txPair.cbegin(), rules.txPair.cend());
    for (const int declaredVersion : {0, 1, 2, 3}) {
        for (int mask = 0; mask < 4; ++mask) {
            QSet<QString> present;
            if (mask & 1) { present.insert(edges.at(0)); }
            if (mask & 2) { present.insert(edges.at(1)); }
            const bool legalPair = mask == 0 || (mask == 3 && declaredVersion >= 1);
            QCOMPARE(mediaSubscribeCombination(rules, present, declaredVersion), legalPair);
            present.insert(QStringLiteral("duplex"));
            for (const bool duplex : {false, true}) {
                QCOMPARE(mediaSubscribeCombination(rules, present, declaredVersion, QJsonValue(duplex)),
                         legalPair && declaredVersion >= rules.duplexVersion);
            }
            QVERIFY(!mediaSubscribeCombination(rules, present, declaredVersion, QJsonValue(1)));
        }
    }
    const QJsonObject conditional = captured().value(QStringLiteral("mediaControl")).toObject()
        .value(QStringLiteral("guiToCore")).toObject().value(QStringLiteral("subscribe")).toObject()
        .value(QStringLiteral("conditional")).toObject();
    QStringList missing;
    for (const QString& key : {QStringLiteral("txMinDbm"), QStringLiteral("txMaxDbm"), QStringLiteral("duplex")}) {
        if (!stringSet(conditional.value(key).toArray()).contains(QStringLiteral("txDisplayVersion"))) {
            missing.append(QStringLiteral("subscribe.") + key + QStringLiteral(" missing txDisplayVersion qualification"));
        }
    }
    // Both members are a declared unit: deleting either must be observable.
    QJsonObject complete = conditional;
    for (const QString& key : {QStringLiteral("txMinDbm"), QStringLiteral("txMaxDbm")}) {
        complete.insert(key, QJsonArray{QStringLiteral("txDisplayVersion")});
    }
    const auto pairComplete = [](const QJsonObject& fields) {
        return fields.contains(QStringLiteral("txMinDbm")) && fields.contains(QStringLiteral("txMaxDbm"));
    };
    QVERIFY(pairComplete(complete));
    for (const QString& edge : {QStringLiteral("txMinDbm"), QStringLiteral("txMaxDbm")}) {
        QJsonObject singleton = complete;
        singleton.remove(edge);
        QVERIFY(!pairComplete(singleton));
    }
    QVERIFY2(missing.isEmpty(), qPrintable(missing.join(QLatin1Char('\n'))));
}

void TstLinkSurfaceManifest::mediaSpectrumTransmitUsesActualCodec()
{
    const QJsonObject recorded = captured().value(QStringLiteral("mediaControl")).toObject()
        .value(QStringLiteral("coreToGui")).toObject().value(QStringLiteral("context")).toObject();
    QStringList drift;
    // tst_remote_spectrum_context.cpp:58-68 supplies the valid available
    // wideband identity; absence and unavailable are legal legacy variants.
    WidebandDisplayContext available;
    available.available = true; available.active = true;
    available.physicalAdcIndex = 0; available.filterChainIndex = 1;
    available.sourceGeneration = 9; available.adcRateHz = 122880000.0;
    const QList<std::optional<WidebandDisplayContext>> widebands{std::nullopt, WidebandDisplayContext{}, available};
    for (const std::optional<WidebandDisplayContext>& wideband : widebands) {
        for (const bool grantPresent : {false, true}) {
            for (const bool negotiated : {false, true}) {
                for (const std::optional<bool> transmit : {std::optional<bool>{}, std::optional<bool>{false}, std::optional<bool>{true}}) {
                    SpectrumContextMessage message = validMediaSpectrum();
                    message.wideband = wideband;
                    if (!grantPresent) { message.grant.reset(); }
                    message.transmit = transmit;
                    const QJsonObject payload = encodeRemoteSpectrumContext(message, negotiated);
                    const bool written = grantPresent && negotiated && transmit.has_value();
                    QCOMPARE(payload.contains(QStringLiteral("transmit")), written);
                    if (written) { QCOMPARE(payload.value(QStringLiteral("transmit")).toBool(), *transmit); }
                    const std::optional<SpectrumContextMessage> decoded = decodeRemoteSpectrumContext(payload, negotiated, written);
                    QCOMPARE(decoded.has_value(), !negotiated || grantPresent);
                    if (decoded && written) { QCOMPARE(decoded->transmit, transmit); }
                    drift.append(mediaCodecDrift(QStringLiteral("context"), payload, recorded));
                    if (written) {
                        QJsonObject complete = recorded;
                        QJsonObject conditional = complete.value(QStringLiteral("conditional")).toObject();
                        conditional.insert(QStringLiteral("transmit"), QJsonArray{QStringLiteral("spectrumGrantVersion"), QStringLiteral("txDisplayVersion")});
                        complete.insert(QStringLiteral("conditional"), conditional);
                        QVERIFY(mediaCodecDrift(QStringLiteral("context"), payload, complete).isEmpty());
                        conditional.remove(QStringLiteral("transmit"));
                        complete.insert(QStringLiteral("conditional"), conditional);
                        QCOMPARE(mediaCodecDrift(QStringLiteral("context"), payload, complete),
                                 QStringList{QStringLiteral("context unrecorded key transmit")});
                    }
                    if (decoded) {
                        QJsonObject extra = payload;
                        extra.insert(QStringLiteral("plantedExtra"), true);
                        QVERIFY(!decodeRemoteSpectrumContext(extra, negotiated, written));
                        for (const QString& key : payload.keys()) {
                            QJsonObject removed = payload;
                            removed.remove(key);
                            QCOMPARE(decodeRemoteSpectrumContext(removed, negotiated, written).has_value(), key == QStringLiteral("wideband"));
                        }
                    }
                }
            }
        }
    }
    const QSet<QString> tags = stringSet(recorded.value(QStringLiteral("conditional")).toObject()
                                           .value(QStringLiteral("transmit")).toArray());
    if (!tags.contains(QStringLiteral("txDisplayVersion")) || !tags.contains(QStringLiteral("spectrumGrantVersion"))) {
        drift.append(QStringLiteral("context.transmit missing grant/TX declaration qualifications"));
    }
    drift.removeDuplicates(); drift.sort();
    QVERIFY2(drift.isEmpty(), qPrintable(drift.join(QLatin1Char('\n'))));
}

void TstLinkSurfaceManifest::mediaNestedLegacyShapesUseActualParser()
{
    const QString source = readSource(QStringLiteral("src/core/session/media/DisplayExtras.cpp"));
    QVERIFY(!source.isEmpty());
    const QJsonObject recorded = captured().value(QStringLiteral("mediaControl")).toObject()
        .value(QStringLiteral("guiToCore")).toObject().value(QStringLiteral("subscribe")).toObject();
    QStringList drift;
    QString error;
    for (const QString& field : {QStringLiteral("activePeakHold"), QStringLiteral("noiseFloor")}) {
        const QSet<QStringList> variants = mediaNestedVariants(source, field, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(variants.size(), 2);
        drift.append(mediaVariantDrift(QStringLiteral("subscribe.") + field, variants, mediaRecordedVariants(recorded, field)));
        for (const QStringList& keys : variants) {
            QJsonObject nested;
            for (const QString& key : keys) {
                if (key == QStringLiteral("enabled")) { nested.insert(key, true); }
                else if (key == QStringLiteral("onTx") || key == QStringLiteral("fastAttack")) { nested.insert(key, false); }
                else if (key == QStringLiteral("holdMs")) { nested.insert(key, 2000); }
                else if (key == QStringLiteral("fallDbPerSec")) { nested.insert(key, 6.0); }
                else if (key == QStringLiteral("shiftDb")) { nested.insert(key, 0.0); }
                else { QFAIL("unrecognized parser-derived nested key; extend valid fixture explicitly"); }
            }
            DisplayExtrasRequest parsed;
            QVERIFY(parseDisplayExtrasRequest(QJsonObject{{field, nested}}, parsed));
            if (field == QStringLiteral("activePeakHold")) {
                QVERIFY(parsed.activePeakHold);
                QCOMPARE(parsed.activePeakHold->onTx, !keys.contains(QStringLiteral("onTx")));
            } else {
                QVERIFY(parsed.noiseFloor);
                QCOMPARE(parsed.noiseFloor->fastAttack, false);
            }
            QJsonObject extra = nested;
            extra.insert(QStringLiteral("plantedExtra"), true);
            QVERIFY(!parseDisplayExtrasRequest(QJsonObject{{field, extra}}, parsed));
            QSet<QStringList> missingVariant = variants;
            missingVariant.remove(keys);
            QCOMPARE(mediaVariantDrift(field, variants, missingVariant).size(), 1);
            for (const QString& key : keys) {
                QJsonObject removed = nested;
                removed.remove(key);
                const bool optional = key == QStringLiteral("onTx") || key == QStringLiteral("fastAttack");
                QCOMPARE(parseDisplayExtrasRequest(QJsonObject{{field, removed}}, parsed), optional);
            }
        }
    }
    QVERIFY2(drift.isEmpty(), qPrintable(drift.join(QLatin1Char('\n'))));
}

void TstLinkSurfaceManifest::mediaAudioBitrateRefusalIncludesLossless()
{
    const QJsonObject recorded = captured().value(QStringLiteral("mediaControl")).toObject()
        .value(QStringLiteral("coreToGui")).toObject().value(QStringLiteral("audio-context")).toObject();
    const QJsonObject conditional = recorded.value(QStringLiteral("conditional")).toObject();
    QStringList drift;
    for (const RemoteAudioProfile profile : {RemoteAudioProfile::Opus, RemoteAudioProfile::Lossless}) {
        for (const bool enabled : {false, true}) {
            RemoteAudioContextMessage message;
            message.connectionId = QStringLiteral("00000000-0000-0000-0000-000000000001");
            message.revision = 1; message.generation = 1; message.ssrc = 1;
            message.enabled = enabled; message.profile = profile;
            // Existing valid profiles: tst_remote_audio_context.cpp:50-53
            // and the exported l16EncoderProfile() production definition.
            if (enabled) { message.encoder = OpusEncoderProfile{48000, 2, 1920, 48000, 20000}; message.losslessEncoder = l16EncoderProfile(); }
            else { message.offReason = RemoteAudioOffReason::ClientDisabled; }
            message.opusBitrateRefusal = opusBitrateNotOfferedReason();
            const QJsonObject payload = encodeRemoteAudioContext(message, true, true);
            QVERIFY(payload.contains(QStringLiteral("opusBitrateRefusal")));
            const std::optional<RemoteAudioContextMessage> decoded = decodeRemoteAudioContext(payload, true, true);
            QVERIFY(decoded);
            QCOMPARE(decoded->profile, message.profile);
            QCOMPARE(decoded->enabled, enabled);
            QCOMPARE(decoded->opusBitrateRefusal, message.opusBitrateRefusal);
            drift.append(mediaCodecDrift(QStringLiteral("audio-context"), payload, recorded));
            QJsonObject extra = payload; extra.insert(QStringLiteral("plantedExtra"), true);
            QVERIFY(!decodeRemoteAudioContext(extra, true, true));
            drift.append(mediaAudioRefusalTagDrift(payload, recorded));
            QJsonObject validTags = recorded;
            QJsonObject validConditional = conditional;
            QJsonArray allowedTags;
            for (const QJsonValue& tag : conditional.value(QStringLiteral("opusBitrateRefusal")).toArray()) {
                if (tag.toString() != QStringLiteral("profile=opus")) { allowedTags.append(tag); }
            }
            validConditional.insert(QStringLiteral("opusBitrateRefusal"), allowedTags);
            validTags.insert(QStringLiteral("conditional"), validConditional);
            QVERIFY(mediaAudioRefusalTagDrift(payload, validTags).isEmpty());
            allowedTags.append(QStringLiteral("profile=opus"));
            validConditional.insert(QStringLiteral("opusBitrateRefusal"), allowedTags);
            validTags.insert(QStringLiteral("conditional"), validConditional);
            QCOMPARE(mediaAudioRefusalTagDrift(payload, validTags).isEmpty(), profile == RemoteAudioProfile::Opus);
            // The separate profileRefusal still belongs only to active Opus.
            message.profileRefusal = RemoteAudioProfileRefusal::NotAllowed;
            const QJsonObject refusal = encodeRemoteAudioContext(message, true, true);
            QCOMPARE(refusal.contains(QStringLiteral("profileRefusal")), profile == RemoteAudioProfile::Opus);
            QVERIFY(decodeRemoteAudioContext(refusal, true, true));
        }
    }
    QVERIFY(stringSet(conditional.value(QStringLiteral("profileRefusal")).toArray()).contains(QStringLiteral("profile=opus")));
    drift.removeDuplicates();
    QVERIFY2(drift.isEmpty(), qPrintable(drift.join(QLatin1Char('\n'))));
}

void TstLinkSurfaceManifest::mediaInlineAndMonitorRepliesAreExact()
{
    const QString source = readSource(QStringLiteral("src/core/session/media/DaemonMediaController.cpp"));
    const QJsonObject media = captured().value(QStringLiteral("mediaControl")).toObject();
    const QJsonObject replies = media.value(QStringLiteral("coreToGui")).toObject();
    QString error;
    for (const auto& [op, function] : {qMakePair(QStringLiteral("iq-stream-context"), QStringLiteral("DaemonMediaController::sendIqContext")),
                                      qMakePair(QStringLiteral("replace"), QStringLiteral("DaemonMediaController::finishReplacement"))}) {
        const QSet<QString> keys = mediaInlineReply(source, function, op, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(keys, stringSet(replies.value(op).toObject().value(QStringLiteral("fields")).toArray()));
        for (const QString& key : keys) {
            QSet<QString> removed = keys; removed.remove(key);
            QVERIFY(removed != keys);
        }
        QString added = source;
        const QString anchor = QStringLiteral("sendControl({{QStringLiteral(\"op\"), QStringLiteral(\"%1\")},").arg(op);
        QVERIFY(added.contains(anchor));
        added.replace(anchor, anchor + QStringLiteral("\n {QStringLiteral(\"plantedExtra\"), true},"));
        const QSet<QString> changed = mediaInlineReply(added, function, op, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(changed - keys, QSet<QString>{QStringLiteral("plantedExtra")});
        QVERIFY(changed != stringSet(replies.value(op).toObject().value(QStringLiteral("fields")).toArray()));
        QString removedSource = source;
        const qsizetype functionAt = removedSource.indexOf(function + QLatin1Char('('));
        QVERIFY(functionAt >= 0);
        const QRegularExpression connectionPair(QString::fromLatin1(
            R"re(\{QStringLiteral\("connectionId"\),[^
]*\},?)re"));
        const QRegularExpressionMatch pair = connectionPair.match(removedSource, functionAt);
        QVERIFY(pair.hasMatch());
        removedSource.remove(pair.capturedStart(), pair.capturedLength());
        const QSet<QString> removedKeys = mediaInlineReply(removedSource, function, op, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(keys - removedKeys, QSet<QString>{QStringLiteral("connectionId")});
        QVERIFY(removedKeys != stringSet(replies.value(op).toObject().value(QStringLiteral("fields")).toArray()));
    }
    for (const TxMonitorRoute route : {TxMonitorRoute::None, TxMonitorRoute::Speakers, TxMonitorRoute::Headphones}) {
        const MonitorAudioMessage message{QStringLiteral("fixture"), 1, route};
        for (const bool reply : {false, true}) {
            const QJsonObject payload = reply ? encodeMonitorAudioContext(message) : encodeMonitorAudioRequest(message);
            const QString op = reply ? QStringLiteral("monitor-audio-context") : QStringLiteral("monitor-audio");
            const QJsonObject declared = (reply ? replies : media.value(QStringLiteral("guiToCore")).toObject()).value(op).toObject();
            QVERIFY(mediaCodecDrift(op, payload, declared).isEmpty());
            const auto decode = [reply](const QJsonObject& p) { return reply ? decodeMonitorAudioContext(p) : decodeMonitorAudioRequest(p); };
            QVERIFY(decode(payload));
            for (const QString& key : payload.keys()) {
                QJsonObject removed = payload; removed.remove(key);
                QVERIFY2(!decode(removed), qPrintable(op + QLatin1Char('.') + key));
            }
            QJsonObject extra = payload; extra.insert(QStringLiteral("plantedExtra"), true);
            QVERIFY(!decode(extra));
            QJsonObject wrong = payload;
            wrong.insert(QStringLiteral("op"), reply ? QStringLiteral("monitor-audio") : QStringLiteral("monitor-audio-context"));
            QVERIFY(!decode(wrong));
        }
    }
}

void TstLinkSurfaceManifest::mediaSourceDriftNegativeControls()
{
    const QString source = readSource(QStringLiteral("src/core/session/media/DaemonMediaController.cpp"));
    QString error;
    const MediaSourceShape start = mediaRequestShape(source, QStringLiteral("DaemonMediaController::handleStart"), &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QJsonObject recorded = mediaRecordedShape(start);
    QVERIFY(mediaShapeDrift(QStringLiteral("start"), start, recorded).isEmpty());
    QJsonObject missing = recorded;
    QJsonObject optional = missing.value(QStringLiteral("conditional")).toObject();
    optional.remove(QStringLiteral("txDisplayVersion")); missing.insert(QStringLiteral("conditional"), optional);
    QCOMPARE(mediaShapeDrift(QStringLiteral("start"), start, missing), QStringList{QStringLiteral("start.conditional missing txDisplayVersion")});
    const QString anchor = QStringLiteral("    QJsonObject legacyShape = control;");
    const qsizetype function = source.indexOf(QStringLiteral("bool DaemonMediaController::handleStart("));
    const qsizetype at = source.indexOf(anchor, function);
    QVERIFY(at >= 0);
    QString added = source;
    added.insert(at + anchor.size(), QStringLiteral("\n    legacyShape.remove(QStringLiteral(\"plantedExtra\"));"));
    const MediaSourceShape planted = mediaRequestShape(added, QStringLiteral("DaemonMediaController::handleStart"), &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(mediaShapeDrift(QStringLiteral("start"), planted, recorded), QStringList{QStringLiteral("start.conditional missing plantedExtra")});
    QString removed = source;
    const QString removal = QStringLiteral("legacyShape.remove(QStringLiteral(\"txDisplayVersion\"));");
    const qsizetype removalAt = removed.indexOf(removal, function);
    QVERIFY(removalAt >= 0); removed.remove(removalAt, removal.size());
    const MediaSourceShape fewer = mediaRequestShape(removed, QStringLiteral("DaemonMediaController::handleStart"), &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(mediaShapeDrift(QStringLiteral("start"), fewer, recorded), QStringList{QStringLiteral("start.conditional unexpected txDisplayVersion")});
    const MediaSourceShape subscribe = mediaRequestShape(source, QStringLiteral("DaemonMediaController::handleSubscribe"), &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QVERIFY(!mediaShapeDrift(QStringLiteral("subscribe"), subscribe, recorded).isEmpty());
    const MediaSubscribeRules rules = mediaSubscribeRules(source, &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    for (const QString& edge : rules.txPair) {
        QString noPairGuard = source;
        const QString guard = QStringLiteral("!control.contains(QStringLiteral(\"%1\"))").arg(edge);
        QVERIFY(noPairGuard.contains(guard));
        noPairGuard.replace(guard, QStringLiteral("false"));
        mediaSubscribeRules(noPairGuard, &error);
        QVERIFY(!error.isEmpty()); error.clear();
    }
    // Unsupported extraction syntax and missing/duplicate anchors fail closed.
    QString unsupported = source;
    unsupported.replace(removal, QStringLiteral("legacyShape.remove(computedKey);"));
    mediaRequestShape(unsupported, QStringLiteral("DaemonMediaController::handleStart"), &error);
    QVERIFY(!error.isEmpty()); error.clear();
    mediaFunction(source, QStringLiteral("DaemonMediaController::missingFunction"), &error);
    QVERIFY(!error.isEmpty()); error.clear();
    mediaFunction(source + source, QStringLiteral("DaemonMediaController::handleStart"), &error);
    QVERIFY(!error.isEmpty());
    const QJsonObject wrongOp{{QStringLiteral("op"), QStringLiteral("replace")}, {QStringLiteral("connectionId"), QStringLiteral("fixture")}};
    QVERIFY(mediaCodecDrift(QStringLiteral("start"), wrongOp, recorded).contains(QStringLiteral("start wrong operation")));
}

void TstLinkSurfaceManifest::limitsMatchTheirSources()
{
    const QJsonObject limits = captured().value(QStringLiteral("limits")).toObject();
    const auto value = [&limits](const char* name) {
        return limits.value(QLatin1String(name)).toObject().value(QStringLiteral("value"));
    };

    // Both ends keep the same heartbeat.
    QCOMPARE(StationClient::kDefaultHeartbeatIntervalMs, StationServer::kDefaultHeartbeatIntervalMs);
    QCOMPARE(StationClient::kDefaultMaxMissedPongs, StationServer::kDefaultMaxMissedPongs);

    // The values held in file-scope constants, read back from their lines.
    const QString daemon =
        readSource(QStringLiteral("src/core/session/media/DaemonMediaController.cpp"));
    const QRegularExpressionMatch endpoints =
        QRegularExpression(QStringLiteral(R"re(constexpr int kMaxEndpoints = (\d+);)re")).match(daemon);
    QVERIFY(endpoints.hasMatch());
    QCOMPARE(value("maxDisplayEndpoints").toInt(), endpoints.captured(1).toInt());

    const QRegularExpressionMatch pixels = QRegularExpression(QStringLiteral(
        R"re(exactInt\(control\.value\(QStringLiteral\("pixels"\)\), (\d+), SpectrumEndpoint::kMaxPixels)re"))
        .match(daemon);
    QVERIFY(pixels.hasMatch());
    QCOMPARE(value("endpointPixels").toObject().value(QStringLiteral("min")).toInt(),
             pixels.captured(1).toInt());

    const QRegularExpressionMatch fps = QRegularExpression(QStringLiteral(
        R"re(exactInt\(control\.value\(QStringLiteral\("fps"\)\), (\d+), (\d+),)re")).match(daemon);
    QVERIFY(fps.hasMatch());
    QCOMPARE(value("endpointFps").toObject().value(QStringLiteral("min")).toInt(),
             fps.captured(1).toInt());
    QCOMPARE(value("endpointFps").toObject().value(QStringLiteral("max")).toInt(),
             fps.captured(2).toInt());
    QCOMPARE(fps.captured(2).toInt(), static_cast<int>(kMaximumSpectrumDisplayFramesPerSecond));

    const QString client = readSource(QStringLiteral("src/core/session/StationClient.cpp"));
    const QRegularExpressionMatch steps = QRegularExpression(QStringLiteral(
        R"re(constexpr int kReconnectBackoffSteps\[\] = \{([^}]*)\};)re")).match(client);
    QVERIFY(steps.hasMatch());
    QJsonArray expected;
    for (const QString& step : steps.captured(1).split(QLatin1Char(','))) {
        expected.append(step.trimmed().toInt() * StationClient::kDefaultReconnectBackoffUnitMs);
    }
    QCOMPARE(value("clientReconnectBackoffMs").toArray(), expected);
}

QTEST_MAIN(TstLinkSurfaceManifest)
#include "tst_link_surface_manifest.moc"
