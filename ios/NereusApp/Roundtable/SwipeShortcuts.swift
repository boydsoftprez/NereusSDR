// NereusSDR for iOS: the Panadapter tab's swipes: from the band's edges and up from the tab bar, each mapped in Setup
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission
// no-port-check: NereusSDR-original.
// Modification history (NereusSDR):
// 2026-10-25: Roundtable plan Task 18, the swipes on the panadapter and what
// each opens. J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
// 2026-10-25: Roundtable plan Task 19, the iPad's defaults: both edges Off.
// J.J. Boyd (KG4VCF), AI-assisted via Claude Code.

import SwiftUI

/// What a swipe on the Panadapter tab opens (spec 11.3, S1). Every page is
/// also reached by its own button (the toolbar, Tools, the flag's
/// Roundtable line): a swipe is only another way in.
enum SwipeTarget: String, CaseIterable {
    case off
    case rxPanel
    case txPanel
    case roundtableMine
    case roundtableAll
    case spotHub
    case tools

    /// Setup's words for the choice.
    var title: String {
        switch self {
        case .off:
            return "Off"
        case .rxPanel:
            return "RX panel"
        case .txPanel:
            return "TX panel"
        case .roundtableMine:
            return "Roundtable, my frequency"
        case .roundtableAll:
            return "Roundtable, all frequencies"
        case .spotHub:
            return "Spot Hub"
        case .tools:
            return "Tools"
        }
    }

    /// The line under a choice that needs one, or nil.
    var detail: String? {
        switch self {
        case .off:
            return "The edge goes back to dragging the band"
        case .txPanel:
            return "Opening it never keys"
        default:
            return nil
        }
    }

    /// Opened on the Tools tab, not over the band.
    var opensTools: Bool {
        self == .roundtableAll || self == .spotHub || self == .tools
    }
}

/// Where a swipe starts.
enum SwipeOrigin: String, CaseIterable, Equatable {
    case leftEdge
    case rightEdge
    case tabBar

    /// The side a page opened by this swipe slides in from.
    var edge: Edge {
        switch self {
        case .leftEdge:
            return .leading
        case .rightEdge:
            return .trailing
        case .tabBar:
            return .bottom
        }
    }
}

/// The swipes' rules, apart from any view: where an edge zone is, which
/// drags are swipes, and when one is far enough to open its page.
enum SwipeShortcuts {
    /// Each edge zone's width, in points (spec 11.3).
    static let edgeZoneWidth: CGFloat = 18
    /// The zones end this far above PTT, which keeps its own touches.
    static let aboveControls: CGFloat = 8
    /// A swipe let go this far in opens its page...
    static let openDistance: CGFloat = 72
    /// ...and so does a quicker one whose flick would carry it this far.
    static let openPredicted: CGFloat = 160

    /// The defaults on a phone: RX panel, TX panel, Roundtable on my
    /// frequency. On an iPad both edges start Off, since the RX and TX
    /// panels already sit in the applet column, and up from the tab bar is
    /// Roundtable, which there widens the applet (spec 11.4, S2).
    static func defaultTarget(_ origin: SwipeOrigin, iPad: Bool = false) -> SwipeTarget {
        switch origin {
        case .leftEdge:
            return iPad ? .off : .rxPanel
        case .rightEdge:
            return iPad ? .off : .txPanel
        case .tabBar:
            return .roundtableMine
        }
    }

    /// The edge zone a drag starting at `start` begins in, on a band
    /// `bandSize` across whose zones end at `zoneFoot` points down;
    /// `leadingInset` and `trailingInset` are the rounded corners a
    /// sideways band reaches under. Nil for anywhere else.
    static func edgeOrigin(start: CGPoint, bandSize: CGSize, zoneFoot: CGFloat, leadingInset: CGFloat = 0,
                           trailingInset: CGFloat = 0) -> SwipeOrigin? {
        guard start.y >= 0, start.y < zoneFoot else {
            return nil
        }
        if start.x < leadingInset + edgeZoneWidth {
            return .leftEdge
        }
        if start.x > bandSize.width - trailingInset - edgeZoneWidth {
            return .rightEdge
        }
        return nil
    }

    /// How far a drag has come in from where it started: right from the
    /// left edge, left from the right edge, up from the tab bar.
    static func inward(_ origin: SwipeOrigin, _ translation: CGSize) -> CGFloat {
        switch origin {
        case .leftEdge:
            return translation.width
        case .rightEdge:
            return -translation.width
        case .tabBar:
            return -translation.height
        }
    }

    /// A drag from a zone is the swipe when it heads in, more in than
    /// across; any other drag there (along the edge, on the dBm scale) is
    /// the band's, as it always was.
    static func claims(_ origin: SwipeOrigin, translation: CGSize) -> Bool {
        let along = origin == .tabBar ? abs(translation.width) : abs(translation.height)
        let into = inward(origin, translation)
        return into > 0 && into > along
    }

    /// Let go `distance` in, with the flick carrying it to `predicted`:
    /// the page opens, or the swipe springs back.
    static func opens(distance: CGFloat, predicted: CGFloat) -> Bool {
        distance >= openDistance || predicted >= openPredicted
    }
}

/// The swipes as the screens share them: the one under the finger, the
/// page the last one opened, and whether anything is up that stops them.
/// The Panadapter tab reads the edges and the root the tab bar; the main
/// screen opens the panels and the Roundtable sheet, the root the pages on
/// the Tools tab.
@MainActor
final class SwipeShortcutModel: ObservableObject {
    /// A swipe under the finger: where it began, what it opens, how far in.
    struct Drag: Equatable {
        var origin: SwipeOrigin
        var target: SwipeTarget
        var distance: CGFloat
    }

    /// A swipe that opened its page; `serial` moves on with each.
    struct Opened: Equatable {
        var serial: Int
        var origin: SwipeOrigin
        var target: SwipeTarget
    }

    @Published private(set) var drag: Drag?
    @Published private(set) var opened: Opened?
    /// A sheet, a panel, the number pad or a flag's menu is up on the
    /// Panadapter tab (the main screen keeps it).
    @Published var panelsUp = false
    /// The Panadapter tab is the one showing (the root keeps it).
    @Published var panadapterShown = true

    let settings: PhoneSettings
    private weak var tuning: BandTuningModel?
    private weak var flagControls: FlagControls?
    private var serial = 0

    init(settings: PhoneSettings) {
        self.settings = settings
    }

    /// The band's own popovers, read as a swipe starts.
    func attach(tuning: BandTuningModel?, flagControls: FlagControls?) {
        self.tuning = tuning
        self.flagControls = flagControls
    }

    /// What a swipe from `origin` opens, as Setup maps it.
    func target(_ origin: SwipeOrigin) -> SwipeTarget {
        settings.swipeTarget(origin)
    }

    /// Nothing is up that stops a swipe.
    var blocked: Bool {
        panelsUp || !panadapterShown || tuning?.pad != nil || tuning?.dialSheetOpen == true
            || tuning?.stepMenuSliceId != nil || flagControls?.open != nil
    }

    /// A swipe from `origin` would open something now.
    func live(_ origin: SwipeOrigin) -> Bool {
        target(origin) != .off && !blocked && drag == nil
    }

    /// A swipe from `origin` begins; false when that one is off or stopped.
    @discardableResult
    func begin(_ origin: SwipeOrigin) -> Bool {
        guard live(origin) else {
            return false
        }
        drag = Drag(origin: origin, target: target(origin), distance: 0)
        return true
    }

    /// The finger is `distance` in.
    func move(_ distance: CGFloat) {
        guard var current = drag else {
            return
        }
        current.distance = max(0, distance)
        drag = current
    }

    /// The finger lifts: far enough, the page opens and ``opened`` says
    /// so (whoever opens it then calls ``finish()``); short, it springs
    /// back. True when it opened.
    @discardableResult
    func end(distance: CGFloat, predicted: CGFloat, reduceMotion: Bool = false) -> Bool {
        guard let current = drag else {
            return false
        }
        if SwipeShortcuts.opens(distance: distance, predicted: predicted) {
            serial += 1
            opened = Opened(serial: serial, origin: current.origin, target: current.target)
            return true
        }
        springBack(reduceMotion: reduceMotion)
        return false
    }

    /// The page the swipe opened is up: the drag is done.
    func finish() {
        drag = nil
    }

    /// A swipe cut short (another touch, the tab changing) springs back.
    func cancel(reduceMotion: Bool = false) {
        springBack(reduceMotion: reduceMotion)
    }

    private func springBack(reduceMotion: Bool) {
        guard drag != nil else {
            return
        }
        if reduceMotion {
            drag = nil
            return
        }
        withAnimation(.easeOut(duration: 0.22)) {
            drag?.distance = 0
        } completion: { [weak self] in
            // A new swipe may have begun meanwhile; only an empty one goes.
            if self?.drag?.distance == 0 {
                self?.drag = nil
            }
        }
    }
}
