// NereusSDR for iOS: the shipped shader source compiles on the device at run time
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Metal
@testable import NereusSDR
import Testing

@Suite("ShaderLibrary")
struct ShaderLibraryTests {
    @Test("the app ships its shaders as source")
    func shipsSource() throws {
        let source = try ShaderLibrary.source()
        #expect(source.contains("solidVertex"))
    }

    @Test("the shaders compile at run time on this device")
    func compilesAtRunTime() throws {
        let device = try #require(MTLCreateSystemDefaultDevice())
        let library = try ShaderLibrary.make(device: device)
        #expect(Set(library.functionNames) == ["solidVertex", "solidFragment"])
    }
}
