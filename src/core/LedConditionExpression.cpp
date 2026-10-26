// =================================================================
// src/core/LedConditionExpression.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original file. Independently implemented from the Thetis LED
// condition behaviour; see LedConditionExpression.h for the language.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-26 - Created for the LED condition by J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
// =================================================================

#include "LedConditionExpression.h"

#include <QLocale>
#include <QMetaType>
#include <QStringList>

namespace NereusSDR {

namespace {

const QString kWhatACanUse = QStringLiteral(
    "A condition can use numbers, \"text\", %names%, + - * /, < <= > >= == !=, && || ! and brackets.");

QString quoted(const QString& text)
{
    return QLatin1Char('"') + text + QLatin1Char('"');
}

struct Value {
    enum class Kind { Number, Text, Flag };
    Kind kind{Kind::Number};
    double number{0.0};
    QString text;
    bool flag{false};

    static Value ofNumber(double n) { Value v; v.kind = Kind::Number; v.number = n; return v; }
    static Value ofText(const QString& t) { Value v; v.kind = Kind::Text; v.text = t; return v; }
    static Value ofFlag(bool b) { Value v; v.kind = Kind::Flag; v.flag = b; return v; }
};

enum class TokenKind { Number, Text, Name, Op, Open, Close, End };

struct Token {
    TokenKind kind{TokenKind::End};
    QString text;      // operator, text value or name
    double number{0.0};
};

bool isNameStart(QChar c)
{
    return c.isLetter() || c == QLatin1Char('_');
}

bool isNamePart(QChar c)
{
    return c.isLetterOrNumber() || c == QLatin1Char('_') || c == QLatin1Char('.');
}

class Parser {
public:
    Parser(const QString& source, const LedConditionLookup& lookup)
        : m_src(source), m_lookup(lookup) {}

    LedConditionResult run()
    {
        LedConditionResult result;
        if (m_src.trimmed().isEmpty()) {
            result.error = QStringLiteral("The condition is empty.");
            return result;
        }
        if (!next()) {
            result.error = m_error;
            return result;
        }
        const std::optional<Value> v = parseOr();
        if (v && m_tok.kind != TokenKind::End) {
            if (m_tok.kind == TokenKind::Close) {
                fail(QStringLiteral("A \")\" has no matching \"(\"."));
            } else {
                fail(quoted(tokenText()) + QStringLiteral(" needs an operator such as && or > before it."));
            }
        }
        if (!v || !m_error.isEmpty()) {
            result.error = m_error;
            return result;
        }
        if (v->kind != Value::Kind::Flag) {
            result.error = v->kind == Value::Kind::Number
                ? QStringLiteral("The condition works out to a number, not to true or false.")
                : QStringLiteral("The condition works out to text, not to true or false.");
            return result;
        }
        result.ok = true;
        result.value = v->flag;
        return result;
    }

private:
    bool fail(const QString& message)
    {
        if (m_error.isEmpty()) {
            m_error = message;
        }
        return false;
    }

    QString tokenText() const
    {
        switch (m_tok.kind) {
        case TokenKind::Number: return m_tok.text;
        case TokenKind::Text:   return QStringLiteral("text");
        case TokenKind::Name:   return QLatin1Char('%') + m_tok.text + QLatin1Char('%');
        case TokenKind::Op:     return m_tok.text;
        case TokenKind::Open:   return QStringLiteral("(");
        case TokenKind::Close:  return QStringLiteral(")");
        case TokenKind::End:    break;
        }
        return {};
    }

    // Reads the next token into m_tok. Every branch moves m_pos forward or
    // stops, so lexing always ends.
    bool next()
    {
        const int n = static_cast<int>(m_src.size());
        while (m_pos < n && m_src.at(m_pos).isSpace()) {
            ++m_pos;
        }
        m_tok = Token{};
        if (m_pos >= n) {
            m_tok.kind = TokenKind::End;
            return true;
        }
        const QChar c = m_src.at(m_pos);
        const QChar after = m_pos + 1 < n ? m_src.at(m_pos + 1) : QChar();

        if (c.isDigit() || (c == QLatin1Char('.') && after.isDigit())) {
            return lexNumber();
        }
        if (c == QLatin1Char('"')) {
            return lexText();
        }
        if (c == QLatin1Char('%')) {
            const int close = static_cast<int>(m_src.indexOf(QLatin1Char('%'), m_pos + 1));
            if (close < 0) {
                return fail(QStringLiteral("A % has no closing %."));
            }
            const QString name = m_src.mid(m_pos + 1, close - m_pos - 1);
            if (name.trimmed().isEmpty()) {
                return fail(QStringLiteral("\"%%\" does not name a value."));
            }
            m_tok.kind = TokenKind::Name;
            m_tok.text = name;
            m_pos = close + 1;
            return true;
        }
        if (c == QLatin1Char('(')) {
            m_tok.kind = TokenKind::Open;
            ++m_pos;
            return true;
        }
        if (c == QLatin1Char(')')) {
            m_tok.kind = TokenKind::Close;
            ++m_pos;
            return true;
        }
        const QString two = m_src.mid(m_pos, 2);
        static const QStringList kTwoCharOps{QStringLiteral("&&"), QStringLiteral("||"), QStringLiteral("=="),
                                             QStringLiteral("!="), QStringLiteral("<="), QStringLiteral(">=")};
        if (kTwoCharOps.contains(two)) {
            m_tok.kind = TokenKind::Op;
            m_tok.text = two;
            m_pos += 2;
            return true;
        }
        static const QString kOneCharOps = QStringLiteral("<>!+-*/");
        if (kOneCharOps.contains(c)) {
            m_tok.kind = TokenKind::Op;
            m_tok.text = QString(c);
            ++m_pos;
            return true;
        }
        if (c == QLatin1Char('&')) {
            return fail(QStringLiteral("\"&\" is not supported. Use \"&&\" for and."));
        }
        if (c == QLatin1Char('|')) {
            return fail(QStringLiteral("\"|\" is not supported. Use \"||\" for or."));
        }
        if (c == QLatin1Char('=')) {
            return fail(QStringLiteral("\"=\" is not supported. Use \"==\" to compare."));
        }
        if (isNameStart(c)) {
            int end = m_pos;
            while (end < n && isNamePart(m_src.at(end))) {
                ++end;
            }
            return fail(quoted(m_src.mid(m_pos, end - m_pos)) + QStringLiteral(" is not supported. ")
                        + kWhatACanUse);
        }
        return fail(quoted(QString(c)) + QStringLiteral(" is not supported. ") + kWhatACanUse);
    }

    bool lexNumber()
    {
        const int n = static_cast<int>(m_src.size());
        const int start = m_pos;
        while (m_pos < n && m_src.at(m_pos).isDigit()) {
            ++m_pos;
        }
        if (m_pos < n && m_src.at(m_pos) == QLatin1Char('.')) {
            ++m_pos;
            while (m_pos < n && m_src.at(m_pos).isDigit()) {
                ++m_pos;
            }
        }
        if (m_pos < n && (m_src.at(m_pos) == QLatin1Char('e') || m_src.at(m_pos) == QLatin1Char('E'))) {
            int p = m_pos + 1;
            if (p < n && (m_src.at(p) == QLatin1Char('+') || m_src.at(p) == QLatin1Char('-'))) {
                ++p;
            }
            if (p < n && m_src.at(p).isDigit()) {
                while (p < n && m_src.at(p).isDigit()) {
                    ++p;
                }
                m_pos = p;
            }
        }
        // A letter, digit, '_' or '.' straight after (5f, 0x10, 1.2.3) is
        // not a number this language reads.
        if (m_pos < n && isNamePart(m_src.at(m_pos))) {
            int end = m_pos;
            while (end < n && isNamePart(m_src.at(end))) {
                ++end;
            }
            return fail(quoted(m_src.mid(start, end - start))
                        + QStringLiteral(" is not a number. Write numbers like 12, -73 or 0.5."));
        }
        const QString text = m_src.mid(start, m_pos - start);
        bool ok = false;
        const double value = QLocale::c().toDouble(text, &ok);
        if (!ok) {
            return fail(quoted(text) + QStringLiteral(" is not a number. Write numbers like 12, -73 or 0.5."));
        }
        m_tok.kind = TokenKind::Number;
        m_tok.text = text;
        m_tok.number = value;
        return true;
    }

    bool lexText()
    {
        const int n = static_cast<int>(m_src.size());
        QString out;
        int p = m_pos + 1;
        while (p < n) {
            const QChar c = m_src.at(p);
            if (c == QLatin1Char('"')) {
                m_tok.kind = TokenKind::Text;
                m_tok.text = out;
                m_pos = p + 1;
                return true;
            }
            if (c == QLatin1Char('\\')) {
                if (p + 1 >= n) {
                    break;
                }
                const QChar e = m_src.at(p + 1);
                if (e == QLatin1Char('"') || e == QLatin1Char('\\')) {
                    out += e;
                } else if (e == QLatin1Char('n')) {
                    out += QLatin1Char('\n');
                } else if (e == QLatin1Char('t')) {
                    out += QLatin1Char('\t');
                } else {
                    return fail(quoted(QStringLiteral("\\") + e)
                                + QStringLiteral(" is not supported in text. Use \\\" \\\\ \\n or \\t."));
                }
                p += 2;
                continue;
            }
            out += c;
            ++p;
        }
        return fail(QStringLiteral("A text in quotes has no closing \"."));
    }

    bool isOp(const char* op) const
    {
        return m_tok.kind == TokenKind::Op && m_tok.text == QLatin1String(op);
    }

    using Step = std::optional<Value> (Parser::*)();

    // One left-to-right level of binary operators.
    std::optional<Value> binaryLevel(std::initializer_list<const char*> ops, Step operand)
    {
        std::optional<Value> left = (this->*operand)();
        while (left) {
            const char* matched = nullptr;
            for (const char* op : ops) {
                if (isOp(op)) {
                    matched = op;
                    break;
                }
            }
            if (!matched) {
                break;
            }
            if (!next()) {
                return std::nullopt;
            }
            const std::optional<Value> right = (this->*operand)();
            if (!right) {
                return std::nullopt;
            }
            left = apply(QString::fromLatin1(matched), *left, *right);
        }
        return left;
    }

    std::optional<Value> parseOr() { return binaryLevel({"||"}, &Parser::parseAnd); }
    std::optional<Value> parseAnd() { return binaryLevel({"&&"}, &Parser::parseEquality); }
    std::optional<Value> parseEquality() { return binaryLevel({"==", "!="}, &Parser::parseRelational); }
    std::optional<Value> parseRelational() { return binaryLevel({"<=", ">=", "<", ">"}, &Parser::parseAdditive); }
    std::optional<Value> parseAdditive() { return binaryLevel({"+", "-"}, &Parser::parseMultiplicative); }
    std::optional<Value> parseMultiplicative() { return binaryLevel({"*", "/"}, &Parser::parseUnary); }

    std::optional<Value> parseUnary()
    {
        if (isOp("!") || isOp("-")) {
            const QString op = m_tok.text;
            if (++m_depth > kLedConditionMaxDepth) {
                fail(QStringLiteral("The condition nests brackets or ! too deeply."));
                return std::nullopt;
            }
            if (!next()) {
                return std::nullopt;
            }
            const std::optional<Value> operand = parseUnary();
            --m_depth;
            if (!operand) {
                return std::nullopt;
            }
            if (op == QLatin1String("!")) {
                if (operand->kind != Value::Kind::Flag) {
                    fail(QStringLiteral("\"!\" works on a true or false part."));
                    return std::nullopt;
                }
                return Value::ofFlag(!operand->flag);
            }
            if (operand->kind != Value::Kind::Number) {
                fail(QStringLiteral("\"-\" works on numbers."));
                return std::nullopt;
            }
            return Value::ofNumber(-operand->number);
        }
        return parsePrimary();
    }

    std::optional<Value> parsePrimary()
    {
        switch (m_tok.kind) {
        case TokenKind::Number: {
            const Value v = Value::ofNumber(m_tok.number);
            return next() ? std::optional<Value>(v) : std::nullopt;
        }
        case TokenKind::Text: {
            const Value v = Value::ofText(m_tok.text);
            return next() ? std::optional<Value>(v) : std::nullopt;
        }
        case TokenKind::Name: {
            const std::optional<Value> v = resolve(m_tok.text);
            if (!v) {
                return std::nullopt;
            }
            return next() ? v : std::nullopt;
        }
        case TokenKind::Open: {
            if (++m_depth > kLedConditionMaxDepth) {
                fail(QStringLiteral("The condition nests brackets or ! too deeply."));
                return std::nullopt;
            }
            if (!next()) {
                return std::nullopt;
            }
            if (m_tok.kind == TokenKind::Close) {
                fail(QStringLiteral("A pair of brackets holds nothing."));
                return std::nullopt;
            }
            const std::optional<Value> inner = parseOr();
            --m_depth;
            if (!inner) {
                return std::nullopt;
            }
            if (m_tok.kind != TokenKind::Close) {
                fail(QStringLiteral("A \"(\" has no matching \")\"."));
                return std::nullopt;
            }
            return next() ? inner : std::nullopt;
        }
        case TokenKind::Close:
            fail(QStringLiteral("A \")\" has no matching \"(\"."));
            return std::nullopt;
        case TokenKind::Op:
            fail(quoted(m_tok.text) + QStringLiteral(" needs a value before it."));
            return std::nullopt;
        case TokenKind::End:
            fail(QStringLiteral("The condition ends where a value should be."));
            return std::nullopt;
        }
        return std::nullopt;
    }

    std::optional<Value> resolve(const QString& name)
    {
        const QString shown = QLatin1Char('%') + name + QLatin1Char('%');
        const std::optional<QVariant> found = m_lookup ? m_lookup(name) : std::nullopt;
        if (!found) {
            fail(shown + QStringLiteral(" is not a reading, a clock value or a data source value."));
            return std::nullopt;
        }
        const QVariant& v = *found;
        if (!v.isValid()) {
            fail(shown + QStringLiteral(" has no value now."));
            return std::nullopt;
        }
        switch (v.typeId()) {
        case QMetaType::Bool:
            return Value::ofFlag(v.toBool());
        case QMetaType::QString:
            return Value::ofText(v.toString());
        case QMetaType::QByteArray:
            return Value::ofText(QString::fromUtf8(v.toByteArray()));
        case QMetaType::Double:
        case QMetaType::Float:
        case QMetaType::Int:
        case QMetaType::UInt:
        case QMetaType::Long:
        case QMetaType::ULong:
        case QMetaType::LongLong:
        case QMetaType::ULongLong:
        case QMetaType::Short:
        case QMetaType::UShort:
        case QMetaType::Char:
        case QMetaType::SChar:
        case QMetaType::UChar:
            return Value::ofNumber(v.toDouble());
        default:
            break;
        }
        fail(shown + QStringLiteral(" is not a number, text or true or false."));
        return std::nullopt;
    }

    std::optional<Value> apply(const QString& op, const Value& a, const Value& b)
    {
        using K = Value::Kind;
        if (op == QLatin1String("&&") || op == QLatin1String("||")) {
            if (a.kind != K::Flag || b.kind != K::Flag) {
                fail(quoted(op) + QStringLiteral(" joins two true or false parts."));
                return std::nullopt;
            }
            return Value::ofFlag(op == QLatin1String("&&") ? (a.flag && b.flag) : (a.flag || b.flag));
        }
        if (op == QLatin1String("==") || op == QLatin1String("!=")) {
            if (a.kind != b.kind) {
                fail(quoted(op) + QStringLiteral(" compares two numbers, two texts or two true or false parts."));
                return std::nullopt;
            }
            bool same = false;
            switch (a.kind) {
            case K::Number: same = a.number == b.number; break;
            case K::Text:   same = a.text == b.text; break;
            case K::Flag:   same = a.flag == b.flag; break;
            }
            return Value::ofFlag(op == QLatin1String("==") ? same : !same);
        }
        if (op == QLatin1String("+") && a.kind == K::Text && b.kind == K::Text) {
            return Value::ofText(a.text + b.text);
        }
        if (a.kind != K::Number || b.kind != K::Number) {
            if (op == QLatin1String("+")) {
                fail(QStringLiteral("\"+\" adds two numbers or joins two texts."));
            } else if (op.startsWith(QLatin1Char('<')) || op.startsWith(QLatin1Char('>'))) {
                fail(quoted(op) + QStringLiteral(" compares two numbers."));
            } else {
                fail(quoted(op) + QStringLiteral(" works on numbers."));
            }
            return std::nullopt;
        }
        if (op == QLatin1String("<"))  { return Value::ofFlag(a.number < b.number); }
        if (op == QLatin1String("<=")) { return Value::ofFlag(a.number <= b.number); }
        if (op == QLatin1String(">"))  { return Value::ofFlag(a.number > b.number); }
        if (op == QLatin1String(">=")) { return Value::ofFlag(a.number >= b.number); }
        if (op == QLatin1String("+"))  { return Value::ofNumber(a.number + b.number); }
        if (op == QLatin1String("-"))  { return Value::ofNumber(a.number - b.number); }
        if (op == QLatin1String("*"))  { return Value::ofNumber(a.number * b.number); }
        // Division follows IEEE doubles, as C# does: x / 0 is infinite.
        return Value::ofNumber(a.number / b.number);
    }

    const QString& m_src;
    const LedConditionLookup& m_lookup;
    int m_pos{0};
    int m_depth{0};
    Token m_tok;
    QString m_error;
};

} // namespace

LedConditionResult evaluateLedCondition(const QString& expression, const LedConditionLookup& lookup)
{
    Parser parser(expression, lookup);
    return parser.run();
}

} // namespace NereusSDR
