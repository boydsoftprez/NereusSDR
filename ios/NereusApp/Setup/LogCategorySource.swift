// NereusSDR for iOS: where the Core's log categories and their labels come from, the phone's list until the Core sends its own
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import NereusMirror

/// Where the Logs page and the Support Bundle page get the Core's log
/// categories and their labels (D95, R-IOS-36). ``CoreLogCategories`` reads
/// the Core's own list and labels (`radio`'s `logCategoryList`, at
/// `logCategoryListVersion` 1); an older Core sends none, and
/// ``PhoneHeldLogCategories`` holds the list on the phone for it.
@MainActor
protocol LogCategorySource {
    /// The categories to list, in order, with their labels. `radio` is the
    /// Core's `radio` object, or nil while there is none.
    func categories(radio: MirrorObject?) -> [CoreLogModel.Category]
}

/// The Core's own logging categories with their labels, in its Support
/// dialog's order, when it sends them (link document section 7.1); the
/// phone's list for an older Core that does not.
struct CoreLogCategories: LogCategorySource {
    let mirror: MirrorStore

    func categories(radio: MirrorObject?) -> [CoreLogModel.Category] {
        guard mirror.capabilityVersion(LogCategoryList.capabilityName) >= 1,
              let json = ToolValue.text(radio?[LogCategoryList.propertyName]),
              let list = LogCategoryList(json: json), !list.categories.isEmpty else {
            return PhoneHeldLogCategories.list
        }
        return list.categories.map { CoreLogModel.Category(id: $0.id, title: $0.label) }
    }
}

/// The Core's log categories held on the phone, in its Support dialog's
/// order and with its names, for a Core that does not send its own.
struct PhoneHeldLogCategories: LogCategorySource {
    static let list: [CoreLogModel.Category] = [
        CoreLogModel.Category(id: "nereus.discovery", title: "Discovery"),
        CoreLogModel.Category(id: "nereus.connection", title: "Connection"),
        CoreLogModel.Category(id: "nereus.protocol", title: "Protocol"),
        CoreLogModel.Category(id: "nereus.receiver", title: "Receiver"),
        CoreLogModel.Category(id: "nereus.audio", title: "Audio"),
        CoreLogModel.Category(id: "nereus.dsp", title: "DSP"),
        CoreLogModel.Category(id: "nereus.spectrum", title: "Spectrum"),
        CoreLogModel.Category(id: "nereus.container", title: "Container"),
        CoreLogModel.Category(id: "nereus.meter", title: "Meter"),
        CoreLogModel.Category(id: "nereus.mmio", title: "MMIO"),
        CoreLogModel.Category(id: "nereus.tci", title: "TCI"),
        CoreLogModel.Category(id: "nereus.spots", title: "Spots"),
    ]

    func categories(radio: MirrorObject?) -> [CoreLogModel.Category] {
        Self.list
    }
}
