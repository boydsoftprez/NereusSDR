// NereusSDR for iOS: the Core's TX Profile prompt and unsaved-changes metadata
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation

public extension SetupDescription {
    // Metadata contract: docs/architecture/2026-09-23-setup-description-v1.md:720-750,
    // resources/setup/audio.json:352-449. The Core owns these words and sources.
    struct ProfilePrompt: Equatable, Sendable {
        public let title: String
        public let label: String
        public let initial: PropertyReference
        public let overwriteTitle: String
        public let overwriteQuestion: String
        public let namesFrom: PropertyReference
    }
    struct ProfileUnsavedChanges: Equatable, Sendable {
        public let title: String
        public let question: String
        public let saveVerb: String
        public let watch: [String]
    }
}

extension SetupDescription {
    static func profilePrompt(_ value: Any?) -> ProfilePrompt? {
        guard let raw = value as? [String: Any], let title = nonempty(raw["title"]),
              let label = nonempty(raw["label"]), let initial = raw["initial"] as? [String: Any],
              let reference = property(initial["$property"]),
              let overwrite = raw["overwrite"] as? [String: Any],
              let overwriteTitle = nonempty(overwrite["title"]),
              let question = nonempty(overwrite["question"]), let names = property(overwrite["namesFrom"]) else { return nil }
        return ProfilePrompt(title: title, label: label, initial: reference,
                             overwriteTitle: overwriteTitle, overwriteQuestion: question, namesFrom: names)
    }
    static func profileUnsavedChanges(_ value: Any?) -> ProfileUnsavedChanges? {
        guard let raw = value as? [String: Any], let title = nonempty(raw["title"]),
              let question = nonempty(raw["question"]), let verb = nonempty(raw["saveVerb"]),
              let watch = raw["watch"] as? [String], !watch.isEmpty,
              watch.allSatisfy({ !$0.isEmpty }), Set(watch).count == watch.count else { return nil }
        return ProfileUnsavedChanges(title: title, question: question, saveVerb: verb, watch: watch)
    }
}
