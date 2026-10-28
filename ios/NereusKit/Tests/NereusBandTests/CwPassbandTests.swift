// NereusSDR for iOS: a CW slice's passband drawn at its origin, the dial plus the CW shift from the station pitch
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// no-port-check: NereusSDR-original.
// Modification history (NereusSDR):
// 2026-10-26: CW transmit, the CW passband at its origin (R-CW-24, D5).
//   J.J. Boyd (KG4VCF), AI-assisted via Claude Code.

import CoreGraphics
import Testing
@testable import NereusBand

/// R-CW-24, D5: in CWU the passband is drawn from the dial minus the pitch,
/// in CWL from the dial plus the pitch, so a CW filter set about the pitch
/// straddles the dial; the dial line and the marker's centre stay on the
/// dial. The pitch is read from the Core's `CWPitch` as the desktop reads it.
@Suite struct CwPassbandTests {
    static let dial = 14_030_000.0

    @Test func theCwuPassbandStraddlesTheDial() {
        let shift = BandSlice.cwShiftHz(modeLabel: "CWU", pitchHz: 600)
        #expect(shift == -600)
        let slice = BandSlice(id: 0, frequencyHz: Self.dial, filterLowHz: 400, filterHighHz: 900, colour: "#00D4FF",
                              lowerSideband: false, passbandOffsetHz: shift)
        // Pitch 600, filter 400 to 900: the dial minus 200 to the dial plus 300.
        #expect(slice.passbandOriginHz == Self.dial - 600)
        #expect(slice.passbandHz == (Self.dial - 200)...(Self.dial + 300))
        #expect(slice.frequencyHz == Self.dial)
        #expect(slice.bandwidthText == "500")
        let marker = SliceMarkers.markers(slices: [slice], activeSliceId: 0, placements: [],
                                           spectrumHeightPoints: 200)[0]
        #expect(marker.centerHz == Self.dial)
        #expect(marker.passbandHz == (Self.dial - 200)...(Self.dial + 300))
    }

    @Test func theCwlPassbandMirrorsIt() {
        let shift = BandSlice.cwShiftHz(modeLabel: "CWL", pitchHz: 600)
        #expect(shift == 600)
        let slice = BandSlice(id: 0, frequencyHz: Self.dial, filterLowHz: -900, filterHighHz: -400, colour: "#00D4FF",
                              lowerSideband: true, passbandOffsetHz: shift)
        #expect(slice.passbandHz == (Self.dial - 300)...(Self.dial + 200))
    }

    @Test func otherModesKeepThePassbandOnTheDial() {
        for label in ["USB", "LSB", "AM", "DIGU", "FM", ""] {
            #expect(BandSlice.cwShiftHz(modeLabel: label, pitchHz: 600) == 0, "\(label)")
            #expect(!BandSlice.isCw(modeLabel: label), "\(label)")
        }
        #expect(BandSlice.isCw(modeLabel: "CWU") && BandSlice.isCw(modeLabel: "CWL"))
        let usb = BandSlice(id: 0, frequencyHz: Self.dial, filterLowHz: 100, filterHighHz: 2900, colour: "#00D4FF",
                            lowerSideband: false)
        #expect(usb.passbandOffsetHz == 0)
        #expect(usb.passbandHz == (Self.dial + 100)...(Self.dial + 2900))
    }

    @Test func thePitchIsReadAsTheDesktopReadsIt() {
        #expect(BandSlice.cwPitchHz(setting: nil) == 600)
        #expect(BandSlice.cwPitchHz(setting: "") == 600)
        #expect(BandSlice.cwPitchHz(setting: "not a number") == 600)
        #expect(BandSlice.cwPitchHz(setting: "650") == 650)
        #expect(BandSlice.cwPitchHz(setting: "654") == 650)
        #expect(BandSlice.cwPitchHz(setting: "655") == 660)
        #expect(BandSlice.cwPitchHz(setting: "100") == 200)
        #expect(BandSlice.cwPitchHz(setting: "5000") == 2250)
        #expect(BandSlice.clampedCwPitchHz(2249) == 2250)
    }

    @Test func anotherDevicesCwPassbandIsDrawnAtItsOrigin() {
        let slice = ForeignSliceMarkers.Slice(id: 1, frequencyHz: Self.dial, filterLowHz: 400, filterHighHz: 900,
                                              colour: "#FF40FF", ownerName: "MacBook Pro", ownerShortName: "MacBook",
                                              passbandOffsetHz: BandSlice.cwShiftHz(modeLabel: "CWU", pitchHz: 600))
        #expect(slice.passbandHz == (Self.dial - 200)...(Self.dial + 300))
        let marker = ForeignSliceMarkers.markers([slice])[0]
        #expect(marker.centerHz == Self.dial)
        #expect(marker.passbandHz == (Self.dial - 200)...(Self.dial + 300))
    }
}
