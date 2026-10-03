// no-port-check: NereusSDR-original.
// =================================================================
// src/core/security/PairingCode.cpp  (NereusSDR)
// =================================================================
//
// See PairingCode.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-24: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
// =================================================================

#include "core/security/PairingCode.h"

#include <QFile>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSet>

// The word list is compiled into NereusCore from resources/pairing.qrc
// (CMakeLists.txt). Q_INIT_RESOURCE must sit outside any namespace; calling
// it from the loader keeps the resource reachable from a Core-only link
// (nereusd, nereus_pairing_peer), as BandPlanManager does for its plans.
static void initPairingResources()
{
    Q_INIT_RESOURCE(pairing);
}

namespace NereusSDR {

namespace {

QStringList loadWords()
{
    initPairingResources();
    QFile file(QStringLiteral(":/pairing/pairing-words-v1.txt"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    QStringList words;
    const QList<QByteArray> lines = file.readAll().split('\n');
    for (const QByteArray& line : lines) {
        const QString word = QString::fromLatin1(line).trimmed();
        if (!word.isEmpty()) {
            words.append(word);
        }
    }
    return words.size() == PairingCode::kWordCount ? words : QStringList{};
}

const QSet<QString>& wordSet()
{
    static const QSet<QString> set = [] {
        const QStringList& words = PairingCode::wordList();
        return QSet<QString>(words.cbegin(), words.cend());
    }();
    return set;
}

} // namespace

const QStringList& PairingCode::wordList()
{
    static const QStringList words = loadWords();
    return words;
}

QString PairingCode::generate(int nameplate)
{
    const QStringList& words = wordList();
    if (nameplate < 1 || nameplate > kMaxNameplate || words.size() != kWordCount) {
        return {};
    }
    QRandomGenerator* random = QRandomGenerator::system();
    const QString first = words.at(static_cast<qsizetype>(random->bounded(kWordCount)));
    const QString second = words.at(static_cast<qsizetype>(random->bounded(kWordCount)));
    return QStringLiteral("%1-%2-%3").arg(nameplate).arg(first, second);
}

QString PairingCode::normalise(const QString& text)
{
    static const QRegularExpression separators(QStringLiteral("[^a-z0-9]+"));
    const QStringList parts =
        text.trimmed().toLower().split(separators, Qt::SkipEmptyParts);
    if (parts.size() != 3) {
        return {};
    }
    // The number: digits only, 1 to kMaxNameplate, without leading zeros.
    const QString& digits = parts.at(0);
    if (digits.size() > 9) {
        return {};
    }
    for (const QChar c : digits) {
        if (!c.isDigit() || c.unicode() > 0x7F) {
            return {};
        }
    }
    bool ok = false;
    const int number = digits.toInt(&ok);
    if (!ok || number < 1 || number > kMaxNameplate) {
        return {};
    }
    if (!wordSet().contains(parts.at(1)) || !wordSet().contains(parts.at(2))) {
        return {};
    }
    return QStringLiteral("%1-%2-%3").arg(number).arg(parts.at(1), parts.at(2));
}

} // namespace NereusSDR
