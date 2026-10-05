// NereusSDR for iOS: Max Bin from the trace this phone displays for the slice's own pan
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// The strongest displayed trace point in a slice's passband. The caller
/// supplies a frame from the slice's confirmed pan subscription, before
/// any visual notch drawing. Its dBm values are already calibrated by the
/// Core, so this adds no offset.
public enum SMeterMaxBin {
    public static func measure(trace: [Float], centerHz: Double, spanHz: Double,
                               lowHz: Double, highHz: Double) -> Double? {
        guard trace.count >= 2, centerHz.isFinite, spanHz.isFinite, spanHz > 0,
              lowHz.isFinite, highHz.isFinite, highHz > lowHz else {
            return nil
        }
        let left = centerHz - spanHz / 2
        let right = centerHz + spanHz / 2
        guard left.isFinite, right.isFinite, highHz >= left, lowHz <= right else {
            return nil
        }
        let hzPerPoint = spanHz / Double(trace.count - 1)
        guard hzPerPoint.isFinite, hzPerPoint > 0 else {
            return nil
        }
        func index(_ hz: Double) -> Int {
            let position = ((hz - left) / hzPerPoint).rounded()
            return Int(min(max(position, 0), Double(trace.count - 1)))
        }
        let first = index(lowHz)
        let last = index(highHz)
        guard last >= first else {
            return nil
        }
        var strongest: Double?
        for sample in trace[first ... last] {
            let dbm = Double(sample)
            if dbm.isFinite, dbm > SMeterReadings.noReadingDbm,
               dbm > (strongest ?? -.infinity) {
                strongest = dbm
            }
        }
        return strongest
    }
}
