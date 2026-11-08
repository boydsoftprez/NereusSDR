// NereusSDR for iOS: where a band's background picture lives, kept relative to Application Support
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// Modification history (NereusSDR):
// 2026-11-05: The picture's path kept relative to Application Support.
// J.J. Boyd (KG4VCF), AI-assisted via Claude Code.

import Foundation

/// Where a band's background picture lives. ``BandDisplaySettings/backgroundImagePath``
/// keeps it relative to Application Support, since an iOS app's container path
/// changes across updates. The value stays on this device: the display store
/// copies it between pans and never sends it to the Core.
public enum BandBackgroundPicture {
    public static let directoryName = "DisplayPictures"

    /// What ``BandDisplaySettings/backgroundImagePath`` keeps for an imported file.
    public static func storedPath(fileName: String) -> String {
        directoryName + "/" + fileName
    }

    /// The file a kept path means. A value from before this was relative is
    /// an absolute path into an old container: its name is found under
    /// ``directoryName`` here, and the path itself is used only when no such
    /// file is there.
    public static func url(forStored path: String, support: URL? = nil,
                           files: FileManager = .default) -> URL {
        guard let support = support ?? files.urls(for: .applicationSupportDirectory, in: .userDomainMask).first else {
            return URL(fileURLWithPath: path)
        }
        guard path.hasPrefix("/") else {
            return support.appendingPathComponent(path)
        }
        let moved = support.appendingPathComponent(directoryName, isDirectory: true)
            .appendingPathComponent((path as NSString).lastPathComponent)
        return files.fileExists(atPath: moved.path) ? moved : URL(fileURLWithPath: path)
    }
}
