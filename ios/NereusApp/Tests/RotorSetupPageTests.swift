// NereusSDR for iOS: Setup, CAT & Network, Rotor: its place in the tree and the presets list it edits
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusMirror
@testable import NereusSDR
import Testing

/// The rotor's setup lives in Setup, CAT & Network, Rotor, after RF-Kit as
/// on the desktop, and its presets list saves as the desktop's does: one
/// `name<TAB>degrees` line per row in order, blank rows skipped, a bad
/// heading refused in the Core's words, and an edit in progress kept while
/// the Core's presets change.
@Suite("Rotor setup page")
@MainActor
struct RotorSetupPageTests {
    /// CAT & Network as a Core describes it, with 4O3A and RF-Kit, and a
    /// Rotor page of its own, which the app's page replaces.
    static let catNetwork = #"{"version":21,"category":{"id":"catNetwork","title":"CAT & Network","where":"mixed"},"pages":["#
        + [("tciServer", "TCI Server"), ("fourO3A", "4O3A"), ("rfKit", "RF-Kit"), ("rotor", "Rotor")].map { page($0.0, $0.1) }
            .joined(separator: ",") + "]}"

    /// One described page with one text row, as a Core sends it.
    static func page(_ id: String, _ title: String) -> String {
        #"{"id":"catNetwork.\#(id)","title":"\#(title)","where":"station","sections":[{"title":"Main","controls":["#
            + #"{"id":"catNetwork.\#(id).name","label":"Name","tooltip":"","kind":"text","applies":"live","#
            + #""requiresDescriptionVersion":15,"binding":{"command":{"verb":"setOperatorText","#
            + #""valueProperty":{"object":"operatorText","name":"name"},"arguments":{"name":{"$controlValue":true}}}}}]}]}"#
    }

    @Test("Setup lists Rotor in CAT & Network right after RF-Kit, once, marked Core")
    func treePlace() throws {
        let alone = try #require(SetupTree.categories.first { $0.title == "CAT & Network" })
        #expect(alone.pages.map(\.title) == ["Rotor", "Data use"])
        #expect(alone.pages.first?.page == .rotor && alone.pages.first?.tag == .core)

        let description = try SetupDescription.parse(json: Self.catNetwork)
        let tree = SetupTree.categories(described: ["catNetwork": description], order: ["catNetwork"], unreadable: [:])
        let category = try #require(tree.first { $0.title == "CAT & Network" })
        #expect(category.pages.map(\.title) == ["TCI Server", "4O3A", "RF-Kit", "Rotor", "Data use"])
        let rotor = try #require(category.pages.first { $0.title == "Rotor" })
        #expect(rotor.destination == .native(.rotor))
        #expect(SetupTree.route(to: .rotor) == [.category("CAT & Network"), .page(.rotor)])
    }

    private func draft(_ rows: [(String, String)]) -> RotorPresetsDraft {
        var draft = RotorPresetsDraft()
        for (name, heading) in rows {
            draft.add()
            let id = draft.rows[draft.rows.count - 1].id
            draft.setName(name, of: id)
            draft.setHeading(heading, of: id)
        }
        return draft
    }

    @Test("Save presets sends the rows in order, name, a tab, the heading; blank rows are skipped")
    func serialization() {
        let presets = draft([("EU", "45"), ("", ""), ("  JA ", " 330 "), ("North", "360"), ("Long\tPath", "12.5"),
                             ("", "  "), ("VK", "250")])
        #expect(presets.serialized() == .success("EU\t45\nJA\t330\nNorth\t0\nLong Path\t12.5\nVK\t250"))
        #expect(RotorPresetsDraft().serialized() == .success(""))
        // A name with no heading, or a heading with no name.
        #expect(draft([("", "90")]).serialized() == .success("\t90"))
    }

    @Test("a bad heading is refused with the Core's words, and nothing is sent")
    func refusals() {
        for (heading, reason) in [("", RotorPresetsDraft.notANumberReason), ("east", RotorPresetsDraft.notANumberReason),
                                  ("nan", RotorPresetsDraft.notANumberReason), ("0x10", RotorPresetsDraft.notANumberReason),
                                  ("400", RotorPresetsDraft.outOfRangeReason), ("-5", RotorPresetsDraft.outOfRangeReason),
                                  ("360.5", RotorPresetsDraft.outOfRangeReason)] {
            var presets = draft([("EU", "45"), ("Bad", heading)])
            let bad = presets.rows[1].id
            #expect(presets.serialized() == .failure(.init(reason: reason, row: bad)), "\(heading)")
            #expect(presets.validated() == nil)
            #expect(presets.message == reason && presets.refusedRow == bad)
        }
        #expect(RotorPresetsDraft.notANumberReason == "That heading is not a number.")
        #expect(RotorPresetsDraft.outOfRangeReason == "That heading is outside the rotor's range.")
        var fixed = draft([("EU", "45")])
        #expect(fixed.validated() == "EU\t45")
        #expect(fixed.message == nil)
    }

    @Test("an edit in progress is kept when the Core's presets change, and follows them again once saved")
    func touchedEditsKept() {
        let core = [RotorModel.Preset(id: 0, name: "EU", degrees: 45), RotorModel.Preset(id: 1, name: "JA", degrees: 330)]
        var presets = RotorPresetsDraft()
        presets.follow(core)
        #expect(presets.rows.map(\.name) == ["EU", "JA"] && presets.rows.map(\.heading) == ["45", "330"])
        #expect(!presets.touched)
        // The same list again keeps the rows as they are.
        let ids = presets.rows.map(\.id)
        presets.follow(core)
        #expect(presets.rows.map(\.id) == ids)

        presets.setHeading("50", of: presets.rows[0].id)
        #expect(presets.touched)
        presets.follow([RotorModel.Preset(id: 0, name: "VK", degrees: 250)])
        #expect(presets.rows.map(\.name) == ["EU", "JA"] && presets.rows.map(\.heading) == ["50", "330"])
        presets.remove(presets.rows[1].id)
        presets.add()
        presets.follow(core)
        #expect(presets.rows.map(\.name) == ["EU", ""])

        presets.saved()
        presets.follow([RotorModel.Preset(id: 0, name: "EU", degrees: 50)])
        #expect(presets.rows.map(\.name) == ["EU"] && presets.rows.map(\.heading) == ["50"])
        var swiped = presets
        swiped.remove(atOffsets: IndexSet(integer: 0))
        #expect(swiped.rows.isEmpty && swiped.touched)
    }
}
