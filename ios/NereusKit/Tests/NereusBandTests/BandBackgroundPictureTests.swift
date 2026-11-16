// NereusSDR for iOS: a band's background picture is found again after an app update moves its container
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// Modification history (NereusSDR):
// 2026-11-05: The picture's path kept relative to Application Support.
// J.J. Boyd (KG4VCF), AI-assisted via Claude Code.

import Foundation
import Testing
@testable import NereusBand

@Suite struct BandBackgroundPictureTests {
    /// An Application Support of its own, holding one imported picture.
    private func support(picture: String) throws -> URL {
        let root = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString, isDirectory: true)
        let directory = root.appendingPathComponent(BandBackgroundPicture.directoryName, isDirectory: true)
        try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
        try Data([1]).write(to: directory.appendingPathComponent(picture))
        return root
    }

    @Test("an import is kept relative to Application Support, with no container path in it")
    func keptRelative() {
        let stored = BandBackgroundPicture.storedPath(fileName: "abc.png")
        #expect(stored == "DisplayPictures/abc.png")
        #expect(!stored.hasPrefix("/"))
    }

    @Test("a relative value opens under this launch's Application Support")
    func relativeOpens() throws {
        let support = try support(picture: "abc.png")
        let url = BandBackgroundPicture.url(forStored: "DisplayPictures/abc.png", support: support)
        #expect(url.path == support.appendingPathComponent("DisplayPictures/abc.png").path)
        #expect(FileManager.default.fileExists(atPath: url.path))
    }

    @Test("an old absolute value from before an update finds its file by name in DisplayPictures")
    func oldAbsoluteFindsItsFile() throws {
        let support = try support(picture: "abc.png")
        let old = "/var/mobile/Containers/Data/Application/OLD-UUID/Library/Application Support/DisplayPictures/abc.png"
        let url = BandBackgroundPicture.url(forStored: old, support: support)
        #expect(url.path == support.appendingPathComponent("DisplayPictures/abc.png").path)
    }

    @Test("an absolute value with no such file here is left as it is")
    func otherAbsoluteStays() throws {
        let support = try support(picture: "abc.png")
        let elsewhere = "/tmp/elsewhere/other.png"
        #expect(BandBackgroundPicture.url(forStored: elsewhere, support: support).path == elsewhere)
    }
}
