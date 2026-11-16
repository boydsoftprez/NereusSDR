// NereusSDR for iOS: the Digital Spectrum Recorder on the pan: where the picture is in time, rewind, LIVE, the hours, SAVE
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// Modification history (NereusSDR):
// 2026-11-16: Recording plan Task 28, the phone's and iPad's Digital Spectrum
//   Recorder. J.J. Boyd (KG4VCF), AI-assisted via Claude Code.

import Foundation
import NereusMirror
import NereusModels
import Observation

/// The Digital Spectrum Recorder on Pan 1 (design 2026-11-15-recording-
/// voice-keyer-dsr sections 3, 6.7 and 6.8, and the phone mockup): the
/// time strip on the right of the waterfall, the hours map beside it, LIVE,
/// SAVE and the clock, on the Core's recorder as ``RecordingsClient`` holds it.
///
/// The picture's top row is ``viewTop`` (seconds since 1970 UTC), or now
/// while live. Within what the band itself still holds, the waterfall shows
/// its own lines (``BandModel/lookBack(seconds:)``); further back the Core
/// draws the rows from its disk with the pan's own display (`dsr.rows`),
/// in chunks, and "Drawing" dims the picture while a quarter or more of it
/// waits. A tap on a signal in the past rewinds the active slice there: it
/// moves to the frequency, plays from that moment, and the picture moves
/// with the sound. LIVE returns it to the radio at the frequency before.
/// A key while rewound is the Core's: it takes the slice live on the
/// frequency it plays, the picture goes live, and LIVE stays red until it
/// is pressed, which still returns to the frequency before.
///
/// An older Core, the recorder off or not connected leave every control in
/// place and disabled with the reason; the strip then moves only through
/// the band's own minutes.
@MainActor
@Observable
final class DsrPanModel {
    nonisolated static let notConnectedReason = "Not connected to a Core."
    nonisolated static let offReason = "The Digital Spectrum Recorder is off. Turn it on in Setup."
    nonisolated static let nothingKeptReason = "The Digital Spectrum Recorder has kept nothing yet."
    /// The bracket SAVE puts up, in points below the picture's top (the phone mockup).
    nonisolated static let bracketTopPoints = 70.0
    nonisolated static let bracketFootPoints = 250.0
    /// The shortest stretch the bracket keeps (the desktop's DsrHoursMap::kMinBracketMs).
    nonisolated static let minBracketSeconds = 30.0
    /// Rows asked for ahead of the sound while it plays (the desktop's 2 s).
    nonisolated static let aheadSeconds = 2.0
    /// "Drawing" shows while this share of the picture waits for the Core.
    nonisolated static let drawingShare = 0.25
    /// The most rows one request asks for (the link's 1 to 4096).
    nonisolated static let mostRows = 4096
    /// A drag that ends this close to now is live again.
    nonisolated static let liveSlackSeconds = 0.3
    /// How long a note stays on the waterfall.
    nonisolated static let noteSeconds = 4.0

    let client: RecordingsClient

    // MARK: What the pan tells it

    /// The pan's display as its subscription carries it; nil before the Core holds one.
    @ObservationIgnored var panView: () -> DsrPanView? = { nil }
    /// The slice that rewinds: the pan's active slice.
    @ObservationIgnored var activeSlice: () -> Int? = { nil }
    /// How far back the band's own lines reach for a whole picture, in seconds.
    @ObservationIgnored var ownSeconds: () -> Double = { 0 }
    /// Moves the band's own waterfall back by seconds (0 is live).
    @ObservationIgnored var lookBack: (Double) -> Void = { _ in }
    /// The clock, seconds since 1970 UTC.
    @ObservationIgnored var clock: () -> Double = { Date().timeIntervalSince1970 }
    /// Seconds a waterfall point stands for (the pan's Update period).
    var secondsPerPoint: Double = 0.1
    /// The waterfall's height in points.
    var waterfallPoints: Double = 0

    // MARK: What the strip shows

    /// The picture's top moment; nil while live.
    private(set) var viewTop: Double?
    /// The rows the Core has drawn, oldest first, for ``rowsView``.
    private(set) var rows: [DsrPastRow] = []
    /// The display the rows were drawn with.
    private(set) var rowsView: DsrPanView?
    /// The rows' layout: first point's frequency and the step, in Hz.
    private(set) var rowsLayout: (firstHz: Double, hzPerBin: Double, count: Int)?
    /// The Core is drawing rows for the picture.
    private(set) var awaitingRows = false
    /// The spectrum at the picture's top moment, while in the past.
    private(set) var pastSpectrum: DsrPastFrames?
    /// Every kept hour, newest first.
    private(set) var hours: [DsrHour] = []
    /// SAVE's bracket, two moments in either order; nil while SAVE is off.
    private(set) var bracket: (Double, Double)?
    /// A note over the waterfall: the Core's words for a save, a refusal or a reason.
    private(set) var note: String?
    /// The PTT took the slice live without LIVE: LIVE stays red until pressed.
    private(set) var liveOwed = false
    /// The time sheet is open.
    var timeSheetOpen = false

    @ObservationIgnored private var rowsTask: Task<Void, Never>?
    @ObservationIgnored private var spectrumTask: Task<Void, Never>?
    @ObservationIgnored private var noteTask: Task<Void, Never>?
    @ObservationIgnored private var hoursAsked = false
    /// The moment played and when it was, from the document or our own rewind.
    @ObservationIgnored private var playAnchor: (utc: Double, at: Double)?
    /// The rewound entry last seen in the document, to tell a key's go-live apart.
    @ObservationIgnored private var lastRewound: RecordingsDocument.Rewound?
    @ObservationIgnored private var askedLive = false
    /// A finger is on the strip: the sound does not move the picture under it.
    @ObservationIgnored private var dragging = false

    init(client: RecordingsClient) {
        self.client = client
    }

    // MARK: Reasons

    /// Why the recorder's controls cannot act now; nil when they can.
    var reason: String? {
        if client.state == .offline {
            return Self.notConnectedReason
        }
        if let older = client.unavailableReason(needs: RecordingsClient.dsrVersion) {
            return older
        }
        guard let dsr = client.dsr else {
            return Self.offReason
        }
        if dsr.stretch != nil {
            return nil
        }
        if !dsr.enabled {
            return Self.offReason
        }
        if !dsr.reason.isEmpty {
            return dsr.reason
        }
        if dsr.oldestUtcMs <= 0 {
            return Self.nothingKeptReason
        }
        return nil
    }

    var usable: Bool { reason == nil }

    /// The oldest moment the picture can show at its top: the oldest kept
    /// (or the open stretch's start) and one picture's height.
    var oldestKept: Double? {
        guard let dsr = client.dsr else {
            return nil
        }
        if let stretch = dsr.stretch {
            return Double(stretch.fromUtcNs) / 1e9
        }
        return dsr.oldestUtcMs > 0 ? Double(dsr.oldestUtcMs) / 1000 : nil
    }

    /// The newest moment there is: now, or the open stretch's end.
    var newest: Double {
        if let stretch = client.dsr?.stretch {
            return Double(stretch.toUtcNs) / 1e9
        }
        return clock()
    }

    /// The picture's height in seconds.
    var pictureSeconds: Double { max(0, waterfallPoints) * secondsPerPoint }

    /// The active slice's past, while it plays one.
    var rewound: RecordingsDocument.Rewound? {
        guard let slice = activeSlice() else {
            return nil
        }
        return client.dsr?.rewound(slice)
    }

    /// The picture shows the past (paused or playing).
    var inPast: Bool { viewTop != nil || client.dsr?.stretch != nil }

    /// LIVE is red: the picture or the sound is in the past, or a key took it live.
    var liveLit: Bool { inPast || rewound != nil || liveOwed }

    /// The moment the slice plays now, while rewound.
    var playedMoment: Double? {
        guard rewound != nil, let playAnchor else {
            return nil
        }
        return playAnchor.utc + (clock() - playAnchor.at)
    }

    /// The top moment drawn: the picture's, or now.
    var top: Double { viewTop ?? newest }

    /// The picture's top lies past what the band holds itself: the Core draws it.
    var coreDraws: Bool {
        guard let viewTop, usable else {
            return false
        }
        return clock() - viewTop > ownSeconds() + 0.5 * secondsPerPoint || client.dsr?.stretch != nil
    }

    /// The share of the picture's rows still waiting for the Core.
    var missingShare: Double {
        guard coreDraws, waterfallPoints >= 1 else {
            return 0
        }
        let total = Int(waterfallPoints)
        var missing = 0
        for y in 0..<total where row(at: top - Double(y) * secondsPerPoint) == nil {
            missing += 1
        }
        return Double(missing) / Double(total)
    }

    /// "Digital Spectrum Recorder: drawing 13:40" while a quarter or more waits.
    var drawingText: String? {
        guard coreDraws, awaitingRows || missingShare >= Self.drawingShare, missingShare > 0 else {
            return nil
        }
        return "Digital Spectrum Recorder: drawing " + Self.hm(top)
    }

    /// The row kept for `moment`: the nearest within half a point.
    func row(at moment: Double) -> DsrPastRow? {
        guard !rows.isEmpty else {
            return nil
        }
        let ns = Int64((moment * 1e9).rounded())
        var low = 0
        var high = rows.count
        while low < high {
            let mid = (low + high) / 2
            if rows[mid].utcNs < ns {
                low = mid + 1
            } else {
                high = mid
            }
        }
        let half = Int64(secondsPerPoint * 0.5e9) + 1
        var best: DsrPastRow?
        for index in [low - 1, low] where rows.indices.contains(index) {
            let distance = abs(rows[index].utcNs - ns)
            if distance <= half, best.map({ abs($0.utcNs - ns) > distance }) ?? true {
                best = rows[index]
            }
        }
        return best
    }

    // MARK: The strip

    /// The strip moves with the finger: up goes back. `startTop` is the top when the drag began.
    func drag(from startTop: Double, by points: Double) {
        dragging = true
        setTop(startTop + points * secondsPerPoint, commit: false)
    }

    /// The finger lifted: the rows are asked for and, while rewound, the sound moves there.
    func dragEnded() {
        dragging = false
        commit()
        if let viewTop, let slice = activeSlice(), rewound != nil {
            let moment = viewTop
            Task { await self.seek(slice: slice, to: moment) }
        }
    }

    /// The hours map: `fraction` 0 at its top (now) to 1 at its foot (the oldest kept).
    func jump(toMapFraction fraction: Double, commit: Bool) {
        guard usable, let oldest = oldestKept else {
            showReasonNote()
            return
        }
        let newest = self.newest
        let moment = newest - min(1, max(0, fraction)) * (newest - oldest)
        setTop(moment + pictureSeconds / 2, commit: commit)
        if commit, let viewTop, let slice = activeSlice(), rewound != nil {
            let moment = viewTop
            Task { await self.seek(slice: slice, to: moment) }
        }
    }

    /// The hours map's mark: where the picture is, as fractions of the map.
    var mapMark: ClosedRange<Double>? {
        guard let oldest = oldestKept, newest > oldest else {
            return nil
        }
        let span = newest - oldest
        let low = (newest - top) / span
        let high = low + pictureSeconds / span
        return max(0, low)...max(0, min(1, high))
    }

    /// "13:05 · 1 h 27 min ago" beside the finger on the map.
    func mapLabel(fraction: Double) -> String? {
        guard let oldest = oldestKept else {
            return nil
        }
        let moment = newest - min(1, max(0, fraction)) * (newest - oldest)
        return Self.hm(moment) + " · " + Self.duration(clock() - moment) + " ago"
    }

    /// The activity shown at `fraction` of the map, 0 to 1.
    func activity(atMapFraction fraction: Double) -> Double {
        guard let oldest = oldestKept else {
            return 0
        }
        let moment = newest - fraction * (newest - oldest)
        let ns = Int64(moment * 1e9)
        return hours.first { $0.startUtcNs <= ns }?.activity ?? 0
    }

    // MARK: Rewind and LIVE

    /// A tap on the waterfall at `pointsDown` from its top, at `hz`. In the
    /// past it rewinds the active slice there and returns true; live it
    /// leaves the tap to tune, and returns false.
    @discardableResult
    func tapWaterfall(pointsDown: Double, hz: Double) -> Bool {
        guard inPast else {
            return false
        }
        guard usable else {
            showReasonNote()
            return true
        }
        guard let slice = activeSlice() else {
            return true
        }
        let moment = top - pointsDown * secondsPerPoint
        Task { await self.rewind(slice: slice, moment: moment, hz: hz) }
        return true
    }

    func rewind(slice: Int, moment: Double, hz: Double) async {
        do {
            try await client.rewind(slice: slice, utcNs: Int64((moment * 1e9).rounded()),
                                    frequencyHz: Int64(hz.rounded()))
            playAnchor = (moment, clock())
            liveOwed = false
            // The clicked row slides to the top; the picture then moves with the sound.
            setTop(moment, commit: true)
        } catch {
            show(error)
        }
    }

    /// LIVE: the picture goes live, and a rewound slice (or one a key took
    /// live) returns to the radio at the frequency it had before.
    func goLive() async {
        let slice = activeSlice()
        let wasRewound = rewound != nil || liveOwed
        viewTop = nil
        pastSpectrum = nil
        lookBack(0)
        liveOwed = false
        guard wasRewound || client.dsr?.stretch != nil, let slice else {
            return
        }
        askedLive = true
        do {
            try await client.live(slice: slice)
        } catch {
            show(error)
        }
    }

    private func seek(slice: Int, to moment: Double) async {
        do {
            try await client.seekPast(slice: slice, utcNs: Int64((moment * 1e9).rounded()))
            playAnchor = (moment, clock())
        } catch {
            show(error)
        }
    }

    /// The Core's document changed: a rewind the Core ended (a key, the end
    /// of what is kept) takes the picture live, and LIVE stays red when it
    /// was not asked for.
    func documentChanged() {
        let now = rewound
        if let now, now != lastRewound {
            playAnchor = (Double(now.utcNs) / 1e9, clock())
        }
        if lastRewound != nil, now == nil {
            if !askedLive {
                liveOwed = true
            }
            viewTop = nil
            pastSpectrum = nil
            lookBack(0)
        }
        if now == nil {
            askedLive = false
        }
        lastRewound = now
    }

    /// Called a few times a second: while the sound plays the picture
    /// follows it, and rows ahead of it are asked for.
    func tick() {
        if !dragging, let played = playedMoment {
            if client.dsr?.stretch == nil, played >= clock() - Self.liveSlackSeconds {
                viewTop = nil
                lookBack(0)
                return
            }
            viewTop = played
            apply()
            if coreDraws {
                askForRows(ahead: true)
            }
        }
    }

    // MARK: Time sheet

    /// Goes to a typed time, "13:05" or "13:05:30" UTC, today or, when later
    /// than now, yesterday. Returns the words to show under the field when it cannot.
    func go(toTyped text: String) -> String? {
        let parts = text.split(separator: ":").map { Int($0.trimmingCharacters(in: .whitespaces)) }
        guard (2...3).contains(parts.count), parts.allSatisfy({ $0 != nil }),
              let hour = parts[0], let minute = parts[1], (0..<24).contains(hour), (0..<60).contains(minute),
              (0..<60).contains(parts.count == 3 ? parts[2] ?? 0 : 0) else {
            return "Type a time like 13:05:00."
        }
        let second = parts.count == 3 ? parts[2] ?? 0 : 0
        let now = clock()
        let day = (now / 86_400).rounded(.down) * 86_400
        var moment = day + Double(hour * 3600 + minute * 60 + second)
        if moment > now {
            moment -= 86_400
        }
        guard usable, let oldest = oldestKept else {
            return reason ?? Self.offReason
        }
        if moment < oldest {
            return "That is older than what is kept (from \(Self.hm(oldest))). Saved stretches are in Tools, Recordings."
        }
        timeSheetOpen = false
        setTop(moment + pictureSeconds / 2, commit: true)
        return nil
    }

    /// The time sheet's line under its title.
    var keptText: String {
        guard let oldest = oldestKept else {
            return reason ?? Self.offReason
        }
        return "Kept from \(Self.hm(oldest)) to now (\(Self.duration(clock() - oldest))). "
            + "Older stretches you saved are in Tools, Recordings."
    }

    /// What the time field starts with: ten minutes ago while live, else the picture's top.
    var typedStart: String { Self.hms(viewTop ?? clock() - 600) }

    func openTimeSheet() {
        guard usable else {
            showReasonNote()
            return
        }
        timeSheetOpen = true
    }

    // MARK: SAVE

    var saving: Bool { bracket != nil }

    /// SAVE: puts the bracket up below the picture's top, or takes it down.
    func toggleSave() {
        guard usable else {
            showReasonNote()
            return
        }
        if bracket != nil {
            bracket = nil
            return
        }
        let end = top - Self.bracketTopPoints * secondsPerPoint
        // Never shorter than the desktop's shortest stretch, however fast the waterfall runs.
        let start = min(top - Self.bracketFootPoints * secondsPerPoint, end - Self.minBracketSeconds)
        bracket = (end, start)
    }

    func cancelSave() {
        bracket = nil
    }

    /// One of the bracket's handles moved to `pointsDown` from the picture's top.
    func moveHandle(_ which: Int, toPointsDown pointsDown: Double) {
        guard var bracket else {
            return
        }
        let moment = min(clock() - 1, top - max(0, min(waterfallPoints, pointsDown)) * secondsPerPoint)
        if which == 0 {
            bracket.0 = moment
        } else {
            bracket.1 = moment
        }
        self.bracket = bracket
    }

    /// The stretch, oldest first.
    var stretch: (from: Double, to: Double)? {
        guard let bracket else {
            return nil
        }
        return (min(bracket.0, bracket.1), max(bracket.0, bracket.1))
    }

    var saveTitle: String {
        guard let stretch else {
            return ""
        }
        return "Save \(Self.hms(stretch.from)) to \(Self.hms(stretch.to)) UTC"
    }

    var saveDetail: String {
        guard let stretch else {
            return ""
        }
        return Self.duration(stretch.to - stretch.from) + ". Drag the yellow handles on the time strip to change it."
    }

    /// The whole pan's size: the recorder's bytes an hour, or what it keeps over how long.
    var wholePanSize: String {
        guard let stretch, let dsr = client.dsr else {
            return ""
        }
        var perSecond = Double(dsr.bytesPerHour) / 3600
        if perSecond <= 0, dsr.newestUtcMs > dsr.oldestUtcMs {
            perSecond = Double(dsr.usedBytes) / (Double(dsr.newestUtcMs - dsr.oldestUtcMs) / 1000)
        }
        return Self.megabytes(perSecond * (stretch.to - stretch.from)) + ", every signal"
    }

    var soundFormat: RecordingsDocument.Format { client.settings?.format ?? .mp3 }

    var soundTitle: String { soundFormat == .wav ? "Sound only (WAV)" : "Sound only (MP3)" }

    /// Slice A's sound, in the Files format: MP3 at 64 kbit/s, WAV 24 kHz 16-bit (V2).
    func soundSize(sliceFrequencyHz: Double?) -> String {
        guard let stretch else {
            return ""
        }
        let wav = soundFormat == .wav
        let bytesPerSecond = wav
            ? Double(RecordingSettingsModel.fileRateHz * RecordingSettingsModel.wavBytesPerSample)
            : Double(RecordingSettingsModel.monoKbitPerSec) * 1000 / 8
        let at = sliceFrequencyHz.map { " at " + Self.frequencyText($0) } ?? ""
        return "Slice A\(at), about " + Self.megabytes(bytesPerSecond * (stretch.to - stretch.from))
    }

    func save(_ kind: DsrStretchKind) async {
        guard let stretch else {
            return
        }
        do {
            let note = try await client.saveStretch(fromUtcNs: Int64((stretch.from * 1e9).rounded()),
                                                    toUtcNs: Int64((stretch.to * 1e9).rounded()), kind: kind)
            bracket = nil
            showNote(note.isEmpty ? (kind == .wholePan ? "Whole pan saved to Recordings on the Core."
                                                       : "Sound saved to Recordings on the Core.") : note)
        } catch {
            show(error)
        }
    }

    // MARK: Asking the Core

    /// The hours, once, when the map first shows.
    func askForHoursOnce() {
        guard !hoursAsked, usable else {
            return
        }
        hoursAsked = true
        Task {
            do {
                hours = try await client.hours()
            } catch {
                hoursAsked = false
            }
        }
    }

    private func setTop(_ moment: Double, commit: Bool) {
        let newest = self.newest
        if client.dsr?.stretch == nil, moment >= newest - Self.liveSlackSeconds {
            viewTop = nil
            pastSpectrum = nil
            lookBack(0)
            return
        }
        let ownOldest = clock() - ownSeconds()
        var lowest = ownOldest
        if usable, let oldest = oldestKept {
            lowest = min(lowest, oldest + pictureSeconds)
        }
        viewTop = max(lowest, min(newest, moment))
        apply()
        if commit {
            self.commit()
        }
    }

    /// The band's own waterfall follows while it holds the picture.
    private func apply() {
        guard let viewTop else {
            lookBack(0)
            return
        }
        lookBack(max(0, clock() - viewTop))
    }

    private func commit() {
        guard viewTop != nil else {
            return
        }
        if coreDraws {
            askForRows(ahead: rewound != nil)
        }
        askForSpectrum()
    }

    /// Asks for the rows the picture lacks, newest first; a newer ask drops an older one.
    private func askForRows(ahead: Bool) {
        guard let view = panView(), let slice = activeSlice() ?? Optional(0), waterfallPoints >= 1 else {
            return
        }
        if rowsView != view {
            rows = []
            rowsView = view
            rowsLayout = nil
        }
        let total = Int(waterfallPoints)
        var newestMissing: Double?
        var oldestMissing: Double?
        for y in 0..<total {
            let moment = top - Double(y) * secondsPerPoint
            if row(at: moment) == nil {
                newestMissing = newestMissing ?? moment
                oldestMissing = moment
            }
        }
        guard var to = newestMissing, let from = oldestMissing else {
            return
        }
        if ahead {
            to = min(newest, to + Self.aheadSeconds)
        }
        if rowsTask != nil, ahead {
            // While the sound plays one ask at a time is enough.
            return
        }
        let count = min(Self.mostRows, max(1, Int(((to - from) / secondsPerPoint).rounded()) + 1))
        rowsTask?.cancel()
        awaitingRows = true
        rowsTask = Task {
            do {
                try await client.pastRows(slice: slice, view: view, fromUtcNs: Int64((from * 1e9).rounded()),
                                          toUtcNs: Int64((to * 1e9).rounded()), rowCount: count) { frames in
                    self.received(frames, view: view)
                }
            } catch is CancellationError {
            } catch {
                if !Task.isCancelled {
                    show(error)
                }
            }
            if !Task.isCancelled {
                awaitingRows = false
                rowsTask = nil
            }
        }
    }

    /// One chunk of rows: kept in time order, at most a few pictures' worth.
    func received(_ frames: DsrPastFrames, view: DsrPanView) {
        guard view == rowsView || rowsView == nil else {
            return
        }
        rowsView = view
        rowsLayout = (frames.firstBinHz, frames.hzPerBin, frames.binCount)
        var byTime = Dictionary(rows.map { ($0.utcNs, $0) }, uniquingKeysWith: { _, new in new })
        for index in 0..<frames.rowCount {
            byTime[frames.utcNs[index]] = DsrPastRow(utcNs: frames.utcNs[index], transmit: frames.transmit[index],
                                                     tenthsDb: Array(frames.row(index)))
        }
        var next = byTime.values.sorted { $0.utcNs < $1.utcNs }
        let most = max(4 * Int(waterfallPoints), 512)
        if next.count > most {
            let centre = Int64(top * 1e9)
            next = Array(next.sorted { abs($0.utcNs - centre) < abs($1.utcNs - centre) }.prefix(most))
                .sorted { $0.utcNs < $1.utcNs }
        }
        rows = next
    }

    private func askForSpectrum() {
        guard coreDraws || rewound != nil, usable, let view = panView(), let slice = activeSlice() else {
            pastSpectrum = nil
            return
        }
        let moment = playedMoment ?? top
        spectrumTask?.cancel()
        spectrumTask = Task {
            if let frame = try? await client.pastSpectrum(slice: slice, view: view,
                                                          utcNs: Int64((moment * 1e9).rounded())),
               !Task.isCancelled {
                pastSpectrum = frame
            }
        }
    }

    // MARK: Notes

    func showReasonNote() {
        if let reason {
            showNote(reason)
        }
    }

    func showNote(_ text: String) {
        note = text
        noteTask?.cancel()
        noteTask = Task {
            try? await Task.sleep(for: .seconds(Self.noteSeconds))
            if !Task.isCancelled {
                note = nil
            }
        }
    }

    private func show(_ error: Error) {
        if let reason = (error as? RecordingsError)?.reason {
            showNote(reason)
        } else {
            showNote("The Core did not answer.")
        }
    }

    // MARK: Words

    nonisolated static func hm(_ moment: Double) -> String {
        let seconds = Int((moment.truncatingRemainder(dividingBy: 86_400) + 86_400).truncatingRemainder(dividingBy: 86_400))
        return String(format: "%02d:%02d", seconds / 3600, seconds % 3600 / 60)
    }

    nonisolated static func hms(_ moment: Double) -> String {
        let seconds = Int((moment.truncatingRemainder(dividingBy: 86_400) + 86_400).truncatingRemainder(dividingBy: 86_400))
        return String(format: "%02d:%02d:%02d", seconds / 3600, seconds % 3600 / 60, seconds % 60)
    }

    /// "3 h 34 min", "2 min 40 s", "45 s", as the mockup words a length.
    nonisolated static func duration(_ seconds: Double) -> String {
        let whole = max(0, Int(seconds.rounded()))
        let hours = whole / 3600
        let minutes = whole % 3600 / 60
        let rest = whole % 60
        if hours > 0 {
            return "\(hours) h \(minutes) min"
        }
        if minutes > 0 {
            return rest > 0 ? "\(minutes) min \(rest) s" : "\(minutes) min"
        }
        return "\(rest) s"
    }

    /// "123 MB", or "1.4 GB" from 1000 MB.
    nonisolated static func megabytes(_ bytes: Double) -> String {
        let mb = max(1, (bytes / 1e6).rounded())
        if mb >= 1000 {
            return String(format: "%.1f GB", mb / 1000)
        }
        return "\(Int(mb)) MB"
    }

    /// "7.236.400" as the flag prints it.
    nonisolated static func frequencyText(_ hz: Double) -> String {
        let whole = Int(hz.rounded())
        return String(format: "%d.%03d.%03d", whole / 1_000_000, whole / 1000 % 1000, whole % 1000)
    }
}

/// One row the Core drew: its moment, whether you were transmitting, and
/// its points in tenths of a dB.
struct DsrPastRow: Equatable, Sendable {
    let utcNs: Int64
    let transmit: Bool
    let tenthsDb: [Int16]
}
