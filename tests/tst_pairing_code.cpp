// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_pairing_code.cpp  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 14 (R-IOS-08): the pairing code and its word list.
//
// The list first, because every code is made of it: exactly 256 lowercase
// words of 4 to 7 letters, no two within one edit of each other (a word
// misheard or mistyped by one letter is not another list word), and none
// sounding like another (checked against a small table of homophones).
// Whether any is offensive was settled by reading the list. The copy
// compiled into the Core is the file in resources/ byte for byte, which
// is also what the iPhone app carries.
//
// Then the code: `<number>-<word>-<word>`, and normalise(), which both
// ends run before hashing: lowercased, trimmed, joined with single
// hyphens whatever the separator, the number without leading zeros, and
// empty for anything that is not a number and two list words.
//
// Codes are made at run time and never printed.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-24: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
// =================================================================

#include <QtTest>

#include <QFile>
#include <QRegularExpression>
#include <QSet>

#include "core/security/PairingCode.h"

using namespace NereusSDR;

namespace {

int editDistance(const QString& a, const QString& b)
{
    QList<int> previous(b.size() + 1);
    for (int j = 0; j <= b.size(); ++j) {
        previous[j] = j;
    }
    for (int i = 1; i <= a.size(); ++i) {
        QList<int> current(b.size() + 1);
        current[0] = i;
        for (int j = 1; j <= b.size(); ++j) {
            const int substitution = previous[j - 1] + (a.at(i - 1) == b.at(j - 1) ? 0 : 1);
            current[j] = std::min({previous[j] + 1, current[j - 1] + 1, substitution});
        }
        previous = current;
    }
    return previous[b.size()];
}

// Pairs of English words that sound alike. A list word and its partner
// here may not both be in the list; the table is small on purpose, and
// covers the words a pairing list is likely to reach for.
const QList<QPair<QString, QString>>& homophones()
{
    static const QList<QPair<QString, QString>> pairs{
        {"bear", "bare"},     {"sail", "sale"},       {"tail", "tale"},
        {"night", "knight"},  {"knot", "not"},        {"flour", "flower"},
        {"hour", "our"},      {"piece", "peace"},     {"steel", "steal"},
        {"brake", "break"},   {"plane", "plain"},     {"whole", "hole"},
        {"meat", "meet"},     {"road", "rode"},       {"week", "weak"},
        {"pair", "pear"},     {"deer", "dear"},       {"sole", "soul"},
        {"hare", "hair"},     {"berry", "bury"},      {"chord", "cord"},
        {"board", "bored"},   {"coarse", "course"},   {"horse", "hoarse"},
        {"maize", "maze"},    {"mail", "male"},       {"pane", "pain"},
        {"reed", "read"},     {"rose", "rows"},       {"scene", "seen"},
        {"suite", "sweet"},   {"tide", "tied"},       {"toad", "towed"},
        {"weight", "wait"},   {"waist", "waste"},     {"whale", "wail"},
        {"wine", "whine"},    {"bolder", "boulder"},  {"cereal", "serial"},
        {"kernel", "colonel"}, {"grate", "great"},    {"heel", "heal"},
        {"muscle", "mussel"}, {"peak", "peek"},       {"pedal", "petal"},
        {"pole", "poll"},     {"rain", "reign"},      {"root", "route"},
        {"site", "sight"},    {"stair", "stare"},     {"yolk", "yoke"},
        {"bread", "bred"},    {"chili", "chilly"},    {"cymbal", "symbol"},
        {"flea", "flee"},     {"idle", "idol"},       {"lesson", "lessen"},
        {"medal", "meddle"},  {"metal", "mettle"},    {"miner", "minor"},
        {"naval", "navel"},   {"plum", "plumb"},      {"prey", "pray"},
        {"throne", "thrown"}, {"vane", "vein"},       {"coral", "choral"},
        {"creek", "creak"},   {"bazaar", "bizarre"},  {"hummus", "humus"},
        {"hangar", "hanger"}, {"tapir", "taper"},     {"gopher", "gofer"},
        {"llama", "lama"},    {"mustard", "mustered"}, {"jewel", "joule"},
        {"moose", "mousse"},  {"desert", "dessert"},  {"timber", "timbre"},
        {"seal", "ceil"},     {"cellar", "seller"},   {"stake", "steak"},
        {"sword", "soared"},  {"beach", "beech"},     {"sun", "son"},
    };
    return pairs;
}

QString generatedCode()
{
    const QString code = PairingCode::generate(7);
    return code;
}

} // namespace

class TstPairingCode : public QObject {
    Q_OBJECT

private slots:
    void theListHas256ShortLowercaseWords()
    {
        const QStringList& words = PairingCode::wordList();
        QCOMPARE(words.size(), 256);
        QCOMPARE(PairingCode::kWordCount, 256);
        static const QRegularExpression shape(QStringLiteral("^[a-z]{4,7}$"));
        for (const QString& word : words) {
            QVERIFY2(shape.match(word).hasMatch(), qPrintable(word));
        }
        QCOMPARE(QSet<QString>(words.cbegin(), words.cend()).size(), 256);
        // The spec's own example is made of list words.
        QVERIFY(words.contains(QStringLiteral("anvil")));
        QVERIFY(words.contains(QStringLiteral("harbor")));
    }

    void noTwoWordsAreOneEditApart()
    {
        const QStringList& words = PairingCode::wordList();
        QStringList close;
        for (int i = 0; i < words.size(); ++i) {
            for (int j = i + 1; j < words.size(); ++j) {
                if (editDistance(words.at(i), words.at(j)) < 2) {
                    close.append(words.at(i) + QLatin1Char('/') + words.at(j));
                }
            }
        }
        QVERIFY2(close.isEmpty(), qPrintable(close.join(QLatin1Char(' '))));
        // The check itself sees one edit.
        QCOMPARE(editDistance(QStringLiteral("harbor"), QStringLiteral("harbour")), 1);
        QCOMPARE(editDistance(QStringLiteral("anvil"), QStringLiteral("anvils")), 1);
        QCOMPARE(editDistance(QStringLiteral("lemon"), QStringLiteral("melon")), 2);
    }

    void noWordSoundsLikeAnother()
    {
        const QStringList& words = PairingCode::wordList();
        const QSet<QString> set(words.cbegin(), words.cend());
        QStringList both;
        for (const auto& [first, second] : homophones()) {
            if (set.contains(first) && set.contains(second)) {
                both.append(first + QLatin1Char('/') + second);
            }
        }
        QVERIFY2(both.isEmpty(), qPrintable(both.join(QLatin1Char(' '))));
    }

    void theCompiledListIsTheFileInResources()
    {
        QFile file(QStringLiteral(NEREUS_SOURCE_DIR "/resources/pairing-words-v1.txt"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QByteArray bytes = file.readAll();
        QCOMPARE(bytes, PairingCode::wordList().join(QLatin1Char('\n')).toLatin1() + '\n');
    }

    void aCodeIsANumberAndTwoListWords()
    {
        const QStringList& words = PairingCode::wordList();
        QSet<QString> seen;
        for (int i = 0; i < 64; ++i) {
            const QString code = generatedCode();
            const QStringList parts = code.split(QLatin1Char('-'));
            QCOMPARE(parts.size(), 3);
            QCOMPARE(parts.at(0), QStringLiteral("7"));
            QVERIFY(words.contains(parts.at(1)));
            QVERIFY(words.contains(parts.at(2)));
            // Already normal.
            QVERIFY(PairingCode::normalise(code) == code);
            seen.insert(code);
        }
        // Drawn at random: 64 draws from 65536 codes are not all one code.
        QVERIFY(seen.size() > 1);
        QCOMPARE(PairingCode::generate(0), QString());
        QCOMPARE(PairingCode::generate(-3), QString());
        QCOMPARE(PairingCode::generate(PairingCode::kMaxNameplate + 1), QString());
        QVERIFY(!PairingCode::generate(PairingCode::kMaxNameplate).isEmpty());
    }

    void normaliseJoinsWhateverSeparatesTheParts()
    {
        QCOMPARE(PairingCode::normalise(QStringLiteral("7 Anvil  harbor")),
                 QStringLiteral("7-anvil-harbor"));
        QCOMPARE(PairingCode::normalise(QStringLiteral("  7-ANVIL-HARBOR \n")),
                 QStringLiteral("7-anvil-harbor"));
        QCOMPARE(PairingCode::normalise(QStringLiteral("7 - anvil -- harbor")),
                 QStringLiteral("7-anvil-harbor"));
        QCOMPARE(PairingCode::normalise(QStringLiteral("7.anvil.harbor")),
                 QStringLiteral("7-anvil-harbor"));
        QCOMPARE(PairingCode::normalise(QStringLiteral("007 anvil harbor")),
                 QStringLiteral("7-anvil-harbor"));
        QCOMPARE(PairingCode::normalise(QStringLiteral("42 harbor anvil")),
                 QStringLiteral("42-harbor-anvil"));
        // A separator before the number is only a separator.
        QCOMPARE(PairingCode::normalise(QStringLiteral("-1-anvil-harbor")),
                 QStringLiteral("1-anvil-harbor"));
    }

    void normaliseRefusesWhatIsNotACode()
    {
        for (const char* text : {"", "7", "7-anvil", "7-anvil-harbor-extra",
                                 "7-anvil-harbour", "anvil-7-harbor", "seven-anvil-harbor",
                                 "0-anvil-harbor", "1234567-anvil-harbor",
                                 "7-anvil-harborx", "7x-anvil-harbor"}) {
            QVERIFY2(PairingCode::normalise(QString::fromUtf8(text)).isEmpty(), text);
        }
    }
};

QTEST_GUILESS_MAIN(TstPairingCode)
#include "tst_pairing_code.moc"
