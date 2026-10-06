// NereusSDR for iOS: compiles the app's Metal shaders from the source it ships
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import Metal

/// The app ships its Metal shaders as source, a bundle resource, and
/// compiles them on the device at run time, so building the app needs no
/// Metal toolchain.
enum ShaderLibrary {
    /// The shader source's resource name, without its extension.
    static let resourceName = "AppShaders"
    /// The bundle directory the shader sources are copied into.
    static let subdirectory = "Shaders"

    enum LoadError: Error, Equatable {
        case sourceMissing
    }

    /// The shader source in `bundle`.
    static func source(in bundle: Bundle = .main) throws -> String {
        guard let url = bundle.url(forResource: resourceName, withExtension: "metal", subdirectory: subdirectory) else {
            throw LoadError.sourceMissing
        }
        return try String(contentsOf: url, encoding: .utf8)
    }

    /// The shaders compiled for `device`.
    static func make(device: any MTLDevice, bundle: Bundle = .main) throws -> any MTLLibrary {
        try device.makeLibrary(source: try source(in: bundle), options: nil)
    }
}
