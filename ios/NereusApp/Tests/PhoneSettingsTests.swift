// NereusSDR for iOS: the phone's own settings keep their values in UserDefaults
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Foundation
import NereusBand
@testable import NereusSDR
import Testing

@Suite("PhoneSettings")
@MainActor
struct PhoneSettingsTests {
    private func freshDefaults() throws -> UserDefaults {
        let name = "PhoneSettingsTests." + UUID().uuidString
        let defaults = try #require(UserDefaults(suiteName: name))
        defaults.removePersistentDomain(forName: name)
        return defaults
    }

    @Test("an unset setting reads its default")
    func defaults() throws {
        let settings = PhoneSettings(defaults: try freshDefaults())
        #expect(settings.bool("dragToTune", default: true))
        #expect(settings.integer("count", default: 3) == 3)
        #expect(settings.double("level", default: 0.5) == 0.5)
        #expect(settings.string("name", default: "A") == "A")
    }

    @Test("a set value persists in the store under the phone's prefix, and a reset forgets it")
    func persists() throws {
        let defaults = try freshDefaults()
        let settings = PhoneSettings(defaults: defaults)
        settings.setBool(false, for: "dragToTune")
        settings.setInteger(7, for: "count")
        settings.setDouble(0.25, for: "level")
        settings.setString("B", for: "name")

        let again = PhoneSettings(defaults: defaults)
        #expect(!again.bool("dragToTune", default: true))
        #expect(again.integer("count", default: 3) == 7)
        #expect(again.double("level", default: 0.5) == 0.25)
        #expect(again.string("name", default: "A") == "B")
        #expect(defaults.object(forKey: "phone.dragToTune") as? Bool == false)

        again.reset("count")
        #expect(again.integer("count", default: 3) == 3)
    }

    @Test("the Touch settings start as spec section 5.1 item 9 lists them and keep what is set")
    func touch() throws {
        let defaults = try freshDefaults()
        let settings = PhoneSettings(defaults: defaults)
        #expect(settings.dragToTune)
        #expect(settings.tapToTune)
        #expect(!settings.snapTapToStep)
        #expect(settings.pinchToZoom)
        #expect(settings.doubleTapAction == .tune)

        settings.dragToTune = false
        settings.tapToTune = false
        settings.snapTapToStep = true
        settings.pinchToZoom = false
        settings.doubleTapAction = .center
        let again = PhoneSettings(defaults: defaults)
        #expect(!again.dragToTune)
        #expect(!again.tapToTune)
        #expect(again.snapTapToStep)
        #expect(!again.pinchToZoom)
        #expect(again.doubleTapAction == .center)
        #expect(defaults.string(forKey: "phone.touch.doubleTapAction") == "center")

        again.doubleTapAction = .none
        #expect(PhoneSettings(defaults: defaults).doubleTapAction == TuneGestures.DoubleTapAction.none)
        // A value this build does not know reads as the default, Tune.
        defaults.set("spin", forKey: "phone.touch.doubleTapAction")
        #expect(PhoneSettings(defaults: defaults).doubleTapAction == .tune)
    }

    @Test("a tap tunes at once by default and waits for a double tap only when Center is chosen")
    func tapTiming() throws {
        let settings = PhoneSettings(defaults: try freshDefaults())
        #expect(settings.tapHandling == .singleAtOnce)
        #expect(!settings.tapHandling.waitsForDoubleTap)
        settings.doubleTapAction = .center
        #expect(settings.tapHandling == .doubleThenSingle)
        #expect(settings.tapHandling.waitsForDoubleTap)
        settings.tapToTune = false
        settings.doubleTapAction = .tune
        #expect(settings.tapHandling == .doubleOnly)
    }
}
