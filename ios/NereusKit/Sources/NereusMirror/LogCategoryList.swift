// NereusSDR for iOS: the Core's logging categories with their Support dialog labels, read from radio.logCategoryList
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusLink

/// The Core's logging categories with their labels (link document section
/// 7.1, `radio`'s `logCategoryList`, `logCategoryListVersion` 1): read-only
/// compact JSON sent in the snapshot only, which a device gets after
/// declaring `logCategoryList` 1 in its hello. The order is the desktop
/// Support dialog's; each `id` is one `logCategories` lists and
/// `support.setLogCategories` carries, each `label` the dialog's checkbox
/// text.
///
/// Unknown keys are ignored; an entry without an id is skipped, and one
/// without a label is named by its id. A value that is not the list's shape
/// reads as nil.
public struct LogCategoryList: Equatable, Sendable {
    public struct Category: Equatable, Sendable {
        public var id: String
        public var label: String

        public init(id: String, label: String) {
            self.id = id
            self.label = label
        }
    }

    /// The feature a device declares in its hello, and the capability the Core answers with.
    public static let featureName = "logCategoryList"
    public static let capabilityName = "logCategoryListVersion"
    /// The `radio` property that carries it.
    public static let propertyName = "logCategoryList"

    /// In the order sent.
    public var categories: [Category]

    public init(categories: [Category]) {
        self.categories = categories
    }

    /// Reads the Core's value; nil when it sent none (an empty string) or
    /// when the value is not the list's shape.
    public init?(json: String) {
        guard !json.isEmpty, case .object(let object)? = try? LinkJSON.parse(json),
              case .array(let items)? = object["categories"] else {
            return nil
        }
        var categories: [Category] = []
        for item in items {
            guard case .object(let fields) = item, case .string(let id)? = fields["id"], !id.isEmpty else {
                continue
            }
            var label = id
            if case .string(let text)? = fields["label"], !text.isEmpty {
                label = text
            }
            categories.append(Category(id: id, label: label))
        }
        self.init(categories: categories)
    }
}
