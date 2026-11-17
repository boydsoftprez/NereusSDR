// NereusSDR for iOS: the Digital Spectrum Recorder on the pan: where the picture is in time, rewind, LIVE, the hours, SAVE
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// Modification history (NereusSDR):
// 2026-11-16: Recording plan Task 28, the phone's and iPad's Digital Spectrum
//   Recorder. J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
// 2026-11-16: Recording plan fix round: the picture follows only this
//   device's own rewind, LIVE reaches every slice it rewound, nothing is owed
//   across a reconnect, moments the Core does not keep are not asked again,
//   the past spectrum keeps up while the sound plays, the hours are asked
//   again as the recorder changes, one save at a time.
//   J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
// 2026-11-16: Rewind plays on release (JJ's design change): letting go of
//   the strip, a pick on the hours map or a typed time plays the active
//   slice from the picture's top at the frequency it is on; a tap in the
//   past tunes as live. J.J. Boyd (KG4VCF), AI-assisted via Claude Code.

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
/// waits. Letting go of the picture in the past (the strip, the hours map,
/// a typed time) plays the active slice from the picture's top at the
/// frequency it is on, and the picture moves with the sound; already
/// playing, the sound moves there. A tap in the past tunes as live; when
/// the slice is not playing the past yet, it then plays from the top.
/// LIVE returns it to the radio at the frequency before.
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
    nonisolated static let nothingKeptReason = "The Digital Spectrum Recorder has kept nothing."
    nonisolated static let noSliceReason = "No slice on this pan to play."
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
    /// The past spectrum is asked for at most this often while the sound
    /// plays, one ask at a time (the desktop's SpectrumWidget kDsrSpectrumIntervalMs, 100 ms).
    nonisolated static let spectrumIntervalSeconds = 0.1
    /// The hours are asked again at least this often while the recorder runs.
    nonisolated static let hoursIntervalSeconds = 3600.0
    /// The most moments remembered as not kept before the memory starts again.
    nonisolated static let mostUnkept = 100_000

    let client: RecordingsClient

    // MARK: What the pan tells it

    /// The pan's display as its subscription carries it; nil before the Core holds one.
    @ObservationIgnored var panView: () -> DsrPanView? = { nil }
    /// The slice that rewinds: the pan's active slice.
    @ObservationIgnored var activeSlice: () -> Int? = { nil }
    /// The active slice's frequency in Hz, which a release plays at.
    @ObservationIgnored var activeSliceHz: () -> Double? = { nil }
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
    /// The slices this device rewound and the Core still plays from the past.
    /// Only these move the picture with their sound, and LIVE returns each.
    private(set) var mine: Set<Int> = []
    /// The slices a key took live after this device rewound them: LIVE stays
    /// red until pressed, and still returns them to the frequency before.
    private(set) var owed: Set<Int> = []
    /// A save is with the Core: both save buttons wait for its answer.
    private(set) var saveInFlight = false
    /// The time sheet is open.
    var timeSheetOpen = false

    @ObservationIgnored private var rowsTask: Task<Void, Never>?
    @ObservationIgnored private var spectrumTask: Task<Void, Never>?
    @ObservationIgnored private var spectrumAskedAt: Double?
    @ObservationIgnored private var noteTask: Task<Void, Never>?
    @ObservationIgnored private var hoursTask: Task<Void, Never>?
    /// What the hours were last asked for: the oldest kept hour, the newest
    /// hour and how many erases there have been; nil before any ask.
    @ObservationIgnored private var hoursKey: String?
    /// The moment each slice played and when it was, from the document or our own rewind.
    @ObservationIgnored private var anchors: [Int: (utc: Double, at: Double)] = [:]
    /// The rewound entries last seen in the document, to tell a key's go-live apart.
    @ObservationIgnored private var lastRewound: [Int: RecordingsDocument.Rewound] = [:]
    /// The slices LIVE was sent for and the Core has not yet listed live.
    @ObservationIgnored private var liveAsked: Set<Int> = []
    /// A finger is on the strip: the sound does not move the picture under it.
    @ObservationIgnored private var dragging = false
    /// The session the state above belongs to: the client's link drops, -1 while offline.
    @ObservationIgnored private var seenSession: Int
    /// The recorder's bytes in use when last seen: a fall is an erase.
    @ObservationIgnored private var seenUsedBytes: Int64?
    @ObservationIgnored private var erases = 0
    /// The recorder's settings that change what it keeps, when last seen.
    @ObservationIgnored private var seenKeeping: String?
    /// Moments asked for that the Core did not keep or refused, as row slots
    /// (the moment over the waterfall's period) of the display they were asked
    /// with: not asked again until the display or the recorder changes.
    @ObservationIgnored private var unkept: Set<Int64> = []
    @ObservationIgnored private var unkeptKey: String?

    init(client: RecordingsClient) {
        self.client = client
        seenSession = client.state == .offline ? -1 : client.linkDrops
    }

    /// The PTT took a slice this device rewound live without LIVE.
    var liveOwed: Bool { sameSession && !owed.isEmpty }

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

    /// The active slice's past, while it plays one (whoever rewound it).
    var rewound: RecordingsDocument.Rewound? {
        guard let slice = activeSlice() else {
            return nil
        }
        return client.dsr?.rewound(slice)
    }

    /// The active slice plays a past this device asked for (its own rewind,
    /// or a saved stretch open on the pan): the picture follows its sound.
    var followsSound: Bool {
        guard rewound != nil, let slice = activeSlice() else {
            return false
        }
        return (sameSession && mine.contains(slice)) || client.dsr?.stretch != nil
    }

    /// The picture shows the past (paused or playing).
    var inPast: Bool { viewTop != nil || client.dsr?.stretch != nil }

    /// LIVE is red: the picture or the active slice's sound is in the past,
    /// another slice this device rewound still plays the past, or a key took one live.
    var liveLit: Bool { inPast || rewound != nil || (sameSession && (!mine.isEmpty || !owed.isEmpty)) }

    /// The moment the slice plays now, while it plays this device's rewind.
    var playedMoment: Double? {
        guard followsSound, let slice = activeSlice(), let anchor = anchors[slice] else {
            return nil
        }
        return anchor.utc + (clock() - anchor.at)
    }

    /// The session this model's state belongs to is the client's now.
    private var sessionKey: Int { client.state == .offline ? -1 : client.linkDrops }
    private var sameSession: Bool { sessionKey >= 0 && sessionKey == seenSession }

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
        for y in 0..<total where waits(top - Double(y) * secondsPerPoint) {
            missing += 1
        }
        return Double(missing) / Double(total)
    }

    /// "Digital Spectrum Recorder: drawing 13:40" while the Core is asked
    /// and a quarter or more of the picture waits for it.
    var drawingText: String? {
        guard coreDraws, awaitingRows, missingShare >= Self.drawingShare else {
            return nil
        }
        return "Digital Spectrum Recorder: drawing " + Self.hm(top)
    }

    /// The moment has no row yet and the Core has not said it keeps none.
    private func waits(_ moment: Double) -> Bool {
        row(at: moment) == nil && !unkept.contains(slot(moment))
    }

    /// The row slot a moment falls in: the moment over the waterfall's period.
    private func slot(_ moment: Double) -> Int64 {
        Int64((moment / max(secondsPerPoint, 0.001)).rounded())
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

    /// The finger lifted: the rows are asked for and the active slice plays from the top.
    func dragEnded() {
        dragging = false
        commit()
        playFromTop()
    }

    /// Rewind plays on release: the active slice plays from the picture's
    /// top at the frequency it is on, or, playing this device's rewind
    /// already, the sound moves there. Why it cannot shows as a note.
    private func playFromTop() {
        guard let viewTop else {
            return
        }
        guard usable else {
            showReasonNote()
            return
        }
        guard let slice = activeSlice(), let hz = activeSliceHz(), hz > 0 else {
            showNote(Self.noSliceReason)
            return
        }
        let moment = viewTop
        if followsSound {
            Task { await self.seek(slice: slice, to: moment) }
        } else {
            Task { await self.rewind(slice: slice, moment: moment, hz: hz) }
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
        if commit {
            playFromTop()
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

    /// A tap on the waterfall at `hz`. It always tunes as a live tap does
    /// (false: the tap is left to tune). In the past, when the active slice
    /// is not playing this device's rewind yet, the slice then plays from
    /// the picture's top at `hz`; playing, the sound follows the tune.
    @discardableResult
    func tapWaterfall(pointsDown: Double, hz: Double) -> Bool {
        guard inPast, usable, !followsSound, let slice = activeSlice() else {
            return false
        }
        let moment = top
        Task { await self.rewind(slice: slice, moment: moment, hz: hz) }
        return false
    }

    /// Rewinds `slice`. A refusal (a slice another device owns, the Core's
    /// words) shows its reason and leaves the picture where it is.
    func rewind(slice: Int, moment: Double, hz: Double) async {
        followSession()
        do {
            try await client.rewind(slice: slice, utcNs: Int64((moment * 1e9).rounded()),
                                    frequencyHz: Int64(hz.rounded()))
            anchors[slice] = (moment, clock())
            mine.insert(slice)
            owed.remove(slice)
            // A LIVE asked before this rewind is done with (the desktop's DsrRewindLink m_liveAsked).
            liveAsked.remove(slice)
            // That moment is the picture's top (already, after a release:
            // nothing more to ask); the picture then moves with the sound.
            if viewTop != moment {
                setTop(moment, commit: true)
            }
        } catch {
            show(error)
        }
    }

    /// LIVE: the picture goes live, and every slice this device rewound
    /// (the active one or another since), or a key took live, returns to
    /// the radio at the frequency it had before. The active slice goes too
    /// when it plays the past; the Core refuses one this device does not
    /// own, and its words show.
    func goLive() async {
        followSession()
        viewTop = nil
        pastSpectrum = nil
        lookBack(0)
        var slices = mine.union(owed)
        if let active = activeSlice(), rewound != nil || client.dsr?.stretch != nil {
            slices.insert(active)
        }
        for slice in slices.sorted() {
            liveAsked.insert(slice)
            do {
                try await client.live(slice: slice)
                mine.remove(slice)
                owed.remove(slice)
            } catch {
                liveAsked.remove(slice)
                show(error)
            }
        }
    }

    private func seek(slice: Int, to moment: Double) async {
        do {
            try await client.seekPast(slice: slice, utcNs: Int64((moment * 1e9).rounded()))
            anchors[slice] = (moment, clock())
        } catch {
            show(error)
        }
    }

    /// The Core's document changed: a rewind of this device's that the Core
    /// ended (a key, the end of what is kept) takes the picture live, and
    /// LIVE stays red when it was not asked for. Another device's rewind
    /// never moves this picture.
    func documentChanged() {
        followSession()
        var now: [Int: RecordingsDocument.Rewound] = [:]
        for entry in client.dsr?.rewound ?? [] {
            now[entry.slice] = entry
        }
        for (slice, entry) in now {
            if entry != lastRewound[slice] {
                anchors[slice] = (Double(entry.utcNs) / 1e9, clock())
            }
            // Listed rewound: a LIVE asked before is done with, and nothing is owed.
            liveAsked.remove(slice)
            owed.remove(slice)
        }
        let active = activeSlice()
        for slice in lastRewound.keys where now[slice] == nil {
            anchors[slice] = nil
            guard mine.remove(slice) != nil else {
                continue
            }
            if !liveAsked.contains(slice) {
                owed.insert(slice)
            }
            if slice == active {
                viewTop = nil
                pastSpectrum = nil
                lookBack(0)
            }
        }
        for slice in liveAsked where now[slice] == nil && lastRewound[slice] != nil {
            liveAsked.remove(slice)
        }
        lastRewound = now
        followRecorder()
    }

    /// Called a few times a second: while the sound plays the picture
    /// follows it, rows ahead of it and its spectrum are asked for, and the
    /// hours are asked again when the recorder has changed.
    func tick() {
        followSession()
        followRecorder()
        askForHoursIfDue()
        if !dragging, let played = playedMoment {
            if client.dsr?.stretch == nil, played >= clock() - Self.liveSlackSeconds {
                viewTop = nil
                pastSpectrum = nil
                lookBack(0)
                return
            }
            viewTop = played
            apply()
            if coreDraws {
                askForRows(ahead: true)
            }
            askForSpectrum(whilePlaying: true)
        }
    }

    /// A drop, a reconnect or another Core: nothing is owed across it, and
    /// the past drawn, the hours and what was not kept are asked again.
    private func followSession() {
        let key = sessionKey
        guard key != seenSession else {
            return
        }
        seenSession = key
        mine = []
        owed = []
        liveAsked = []
        lastRewound = [:]
        anchors = [:]
        seenUsedBytes = nil
        seenKeeping = nil
        forgetPast()
    }

    /// Erase (a fall in the bytes kept) or a change in what the recorder
    /// keeps drops the past drawn and the hours; rows older than the oldest
    /// kept go as the loop erases them.
    private func followRecorder() {
        guard let dsr = client.dsr else {
            return
        }
        let keeping = "\(dsr.enabled)|\(dsr.disk)|\(dsr.sizeLimitGb)|\(dsr.stretch?.id ?? "")"
        if let seenUsedBytes, dsr.usedBytes < seenUsedBytes {
            erases += 1
            forgetPast()
        } else if let seenKeeping, seenKeeping != keeping {
            forgetPast()
        }
        seenUsedBytes = dsr.usedBytes
        seenKeeping = keeping
        if dsr.stretch == nil, !rows.isEmpty {
            let oldestNs = dsr.oldestUtcMs * 1_000_000
            if oldestNs <= 0 {
                rows = []
            } else if rows[0].utcNs < oldestNs {
                rows.removeAll { $0.utcNs < oldestNs }
            }
        }
    }

    /// Drops the rows drawn, the hours and the moments not kept.
    private func forgetPast() {
        rowsTask?.cancel()
        rowsTask = nil
        awaitingRows = false
        rows = []
        rowsView = nil
        rowsLayout = nil
        unkept = []
        unkeptKey = nil
        spectrumTask?.cancel()
        spectrumTask = nil
        spectrumAskedAt = nil
        pastSpectrum = nil
        hoursTask?.cancel()
        hoursTask = nil
        hours = []
        hoursKey = nil
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
        playFromTop()
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

    /// The handle's step for VoiceOver's adjust: one tick of the strip, ten points.
    nonisolated static let handleStepPoints = 10.0

    /// Moves one of the bracket's handles a step later (up) or earlier (down),
    /// as dragging it would: the way to it without a drag.
    func stepHandle(_ which: Int, later: Bool) {
        guard let bracket else {
            return
        }
        let moment = which == 0 ? bracket.0 : bracket.1
        let pointsDown = (top - moment) / max(secondsPerPoint, 0.001)
        moveHandle(which, toPointsDown: pointsDown + (later ? -Self.handleStepPoints : Self.handleStepPoints))
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

    /// One save at a time: a second tap while the Core answers the first sends nothing.
    func save(_ kind: DsrStretchKind) async {
        guard let stretch, !saveInFlight else {
            return
        }
        saveInFlight = true
        defer {
            saveInFlight = false
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

    /// The hours, when the recorder becomes usable and again when what it
    /// keeps has moved: a new oldest hour, a new hour now (at most once an
    /// hour while it runs), an erase, a reconnect. One ask at a time.
    func askForHoursIfDue() {
        followSession()
        followRecorder()
        guard usable, hoursTask == nil, let oldest = oldestKept else {
            return
        }
        let hour = Self.hoursIntervalSeconds
        let key = "\(Int64((oldest / hour).rounded(.down)))|\(Int64((clock() / hour).rounded(.down)))|\(erases)"
        guard key != hoursKey else {
            return
        }
        hoursKey = key
        hoursTask = Task {
            do {
                let answer = try await client.hours()
                if !Task.isCancelled {
                    hours = answer
                }
            } catch {
                if !Task.isCancelled {
                    // Asked again on the next tick.
                    hoursKey = nil
                }
            }
            if !Task.isCancelled {
                hoursTask = nil
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
        let memory = "\(view)|\(secondsPerPoint)"
        if unkeptKey != memory || unkept.count > Self.mostUnkept {
            unkept = []
            unkeptKey = memory
        }
        let total = Int(waterfallPoints)
        var newestMissing: Double?
        var oldestMissing: Double?
        for y in 0..<total {
            let moment = top - Double(y) * secondsPerPoint
            if waits(moment) {
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
            var answered = true
            do {
                try await client.pastRows(slice: slice, view: view, fromUtcNs: Int64((from * 1e9).rounded()),
                                          toUtcNs: Int64((to * 1e9).rounded()), rowCount: count) { frames in
                    self.received(frames, view: view)
                }
            } catch is CancellationError {
                answered = false
            } catch {
                if !Task.isCancelled {
                    show(error)
                }
            }
            if !Task.isCancelled {
                if answered {
                    // What the Core left out, or refused, is not kept: not asked again.
                    rememberUnkept(from: from, to: to, memory: memory)
                }
                awaitingRows = false
                rowsTask = nil
            }
        }
    }

    /// The moments from `from` to `to` that still have no row once the Core has answered.
    private func rememberUnkept(from: Double, to: Double, memory: String) {
        guard unkeptKey == memory, to >= from else {
            return
        }
        let spp = max(secondsPerPoint, 0.001)
        var moment = to
        while moment >= from - spp / 2 {
            if row(at: moment) == nil {
                unkept.insert(slot(moment))
            }
            moment -= spp
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

    /// The spectrum at the picture's top or the moment played. While the
    /// sound plays it is asked again at the desktop's interval, one ask at a
    /// time; a moved picture replaces any ask under way.
    private func askForSpectrum(whilePlaying: Bool = false) {
        guard coreDraws || followsSound, usable, let view = panView(), let slice = activeSlice() else {
            pastSpectrum = nil
            return
        }
        let now = clock()
        if whilePlaying {
            if spectrumTask != nil {
                return
            }
            if let spectrumAskedAt, now - spectrumAskedAt < Self.spectrumIntervalSeconds {
                return
            }
        }
        spectrumAskedAt = now
        let moment = playedMoment ?? top
        spectrumTask?.cancel()
        spectrumTask = Task {
            let frame = try? await client.pastSpectrum(slice: slice, view: view,
                                                       utcNs: Int64((moment * 1e9).rounded()))
            if !Task.isCancelled {
                if let frame {
                    pastSpectrum = frame
                }
                spectrumTask = nil
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
        if let error = error as? RecordingsError, let reason = error.reason {
            showNote(reason)
        } else if (error as? RecordingsError) == .notConnected {
            showNote(Self.notConnectedReason)
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
