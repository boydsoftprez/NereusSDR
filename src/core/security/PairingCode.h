#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// src/core/security/PairingCode.h  (NereusSDR)
// =================================================================
//
// The pairing code (iPhone app plan Task 14, R-IOS-08; the pairing design,
// docs/architecture/2026-08-02-remote-station-identity-and-pairing-design.md
// section 4.3): a number and two words, for example `7-anvil-harbor`,
// short enough to read out over the phone and type once.
//
//   - The number is the nameplate: the rendezvous's number for this
//     Core's pairing (Task 27). Until the rendezvous exists the pairing
//     window picks one from 1 to 99.
//   - The words come from resources/pairing-words-v1.txt: 256 lowercase
//     words of 4 to 7 letters, no two within one edit of each other and
//     no two that sound alike, so a word misheard or mistyped by one
//     letter is not another word of the list. Each word carries 8 bits;
//     the code's strength is not in its length but in the exchange it
//     feeds (SpakeExchange): a wrong guess burns the code, and the next
//     one appears only after a wait that doubles (PairingWindow).
//   - Normalised by lowercasing, trimming, and joining the three parts
//     with single hyphens whatever separator was typed. The normalised
//     text, as UTF-8, is the password both ends hash; the iPhone app
//     normalises exactly the same way (its PairingCodeText).
//
// A code is a secret while it is live: never logged, never written to a
// fixture. It is shown on the Core's console, on its status page and on
// the Remote Access page of a desktop running the Core, and sent over the
// link only to a connection signed in with a paired device's key.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-24: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
// =================================================================

#include <QString>
#include <QStringList>

namespace NereusSDR {

class PairingCode {
public:
    /// Words in resources/pairing-words-v1.txt.
    static constexpr int kWordCount = 256;
    /// The nameplate's range: whole numbers from 1 to this, written without
    /// leading zeros. Generous: a rendezvous hands out the lowest free one.
    static constexpr int kMaxNameplate = 999999;

    /// A fresh code, `<nameplate>-<word>-<word>`, both words drawn from the
    /// operating system's generator. Empty when `nameplate` is out of range
    /// or the word list did not load.
    static QString generate(int nameplate);

    /// The code as both ends hash it: lowercased, trimmed, the three parts
    /// joined with single hyphens whatever ran between them (spaces,
    /// hyphens, dots, several of them). Empty when the text is not a
    /// number and two words of the list (a word the list does not hold is
    /// a typing mistake, and must not cost the operator the code). The
    /// number keeps no leading zeros.
    static QString normalise(const QString& text);

    /// The 256 words, in the file's order. Empty only if the resource is
    /// missing from the build.
    static const QStringList& wordList();
};

} // namespace NereusSDR
