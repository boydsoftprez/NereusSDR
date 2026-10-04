// NereusSDR for iOS: the Core's open question over whatever screen is showing, in a window of its own above the app's
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Combine
import NereusMirror
import SwiftUI
import UIKit

/// Where the Core's open question (`confirm.request`) is asked (D85): over
/// whatever this phone is showing, any tab, a page pushed on it, a panel
/// or a sheet, so it is answered where the change was made. It lives in a
/// window of its own one level above the app's window, so no screen and no
/// sheet the app presents can cover it, and it is the only place the
/// question is drawn, so it is never shown twice.
///
/// The question covers everything above the tab bar and rises from the
/// tab bar's top edge, where it rose on the band. The tab bar stays in
/// reach: a tab change leaves the question up, over the new tab.
///
/// Put it behind the tab bar, which it measures:
/// `TabBar(...).background(ConfirmationWindowAnchor(app: model))`.
struct ConfirmationWindowAnchor: UIViewRepresentable {
    let devices: SeveralDevicesClient
    let main: MainScreenModel

    init(app: AppModel) {
        devices = app.devices
        main = app.main
    }

    func makeCoordinator() -> ConfirmationPresenter {
        ConfirmationPresenter(devices: devices, main: main)
    }

    func makeUIView(context: Context) -> AnchorView {
        let view = AnchorView()
        view.presenter = context.coordinator
        return view
    }

    func updateUIView(_ view: AnchorView, context: Context) {}

    static func dismantleUIView(_ view: AnchorView, coordinator: ConfirmationPresenter) {
        coordinator.detach()
    }

    /// Behind the tab bar: it finds the app's window and the tab bar's top
    /// edge, and follows both as the phone turns.
    final class AnchorView: UIView {
        weak var presenter: ConfirmationPresenter?

        override init(frame: CGRect) {
            super.init(frame: frame)
            isUserInteractionEnabled = false
            backgroundColor = .clear
        }

        @available(*, unavailable)
        required init?(coder: NSCoder) {
            fatalError("init(coder:) is not used")
        }

        override func didMoveToWindow() {
            super.didMoveToWindow()
            if window == nil {
                presenter?.detach()
            } else {
                presenter?.attach(anchor: self)
            }
        }

        override func layoutSubviews() {
            super.layoutSubviews()
            presenter?.follow()
        }
    }
}

/// The question's window and when it shows: while the Core holds open a
/// question this layer asks (``ConfirmationLayer/shows(_:takesTransmit:)``),
/// or the phone asks its own before taking transmit.
@MainActor
final class ConfirmationPresenter: ObservableObject {
    /// The height from the window's top to the tab bar's top edge, in the
    /// window's points; the question covers it.
    @Published private(set) var coveredHeight: CGFloat = 0

    let devices: SeveralDevicesClient
    let main: MainScreenModel
    private(set) var window: ConfirmationWindow?
    private weak var anchor: UIView?
    private var watch: AnyCancellable?

    init(devices: SeveralDevicesClient, main: MainScreenModel) {
        self.devices = devices
        self.main = main
    }

    /// Opens the question's window over the anchor's window, once: a second
    /// anchor in the same window finds it there and adds none.
    func attach(anchor: UIView) {
        guard let host = anchor.window, !(host is ConfirmationWindow), let scene = host.windowScene else {
            return
        }
        self.anchor = anchor
        if let window, window.host === host {
            follow()
            return
        }
        detach()
        self.anchor = anchor
        let taken = scene.windows.contains { other in
            guard let other = other as? ConfirmationWindow else {
                return false
            }
            return other.host === host && !other.isDismantled
        }
        guard !taken else {
            return
        }
        let window = ConfirmationWindow(windowScene: scene)
        window.host = host
        window.overrideUserInterfaceStyle = .dark
        window.backgroundColor = .clear
        let root = UIHostingController(rootView: ConfirmationWindowContent(presenter: self, devices: devices,
                                                                             main: main))
        root.view.backgroundColor = .clear
        window.rootViewController = root
        window.isHidden = true
        self.window = window
        watch = devices.$question.combineLatest(main.take.$asked).sink { [weak self] question, asked in
            self?.show(question, asked: asked)
        }
        follow()
    }

    /// Closes the question's window; the question itself stays the Core's.
    func detach() {
        watch = nil
        if let window {
            window.isHidden = true
            window.isDismantled = true
            window.rootViewController = nil
        }
        window = nil
        anchor = nil
    }

    /// Follows the app's window and the tab bar's top edge.
    func follow() {
        guard let window, let host = window.host, let anchor else {
            return
        }
        window.windowLevel = UIWindow.Level(rawValue: host.windowLevel.rawValue + 1)
        if window.frame != host.frame {
            window.frame = host.frame
        }
        let top = anchor.convert(anchor.bounds, to: host).minY
        if coveredHeight != top {
            coveredHeight = top
        }
        window.coveredHeight = top
    }

    private func show(_ question: SeveralDevices.Question?, asked: TransmitTakeModel.Asked?) {
        guard let window else {
            return
        }
        let shows = asked != nil || (question.map { ConfirmationLayer.shows($0.kind, takesTransmit: true) } ?? false)
        if shows {
            follow()
        }
        let appearing = shows && window.isHidden
        window.isHidden = !shows
        if appearing {
            // VoiceOver moves to the question.
            UIAccessibility.post(notification: .screenChanged, argument: nil)
        }
    }
}

/// The question's window: it takes touches over the part it covers, and
/// lets those on the tab bar through to the app's window below it.
final class ConfirmationWindow: UIWindow {
    /// The app's window it sits over.
    weak var host: UIWindow?
    /// The height from the top it covers.
    var coveredHeight: CGFloat = 0
    /// Its anchor has gone; it shows nothing again.
    var isDismantled = false

    override func hitTest(_ point: CGPoint, with event: UIEvent?) -> UIView? {
        guard point.y < coveredHeight else {
            return nil
        }
        return super.hitTest(point, with: event)
    }
}

/// The window's content: the question's layer over the covered height.
private struct ConfirmationWindowContent: View {
    @ObservedObject var presenter: ConfirmationPresenter
    @ObservedObject var devices: SeveralDevicesClient
    let main: MainScreenModel
    @ObservedObject private var take: TransmitTakeModel

    init(presenter: ConfirmationPresenter, devices: SeveralDevicesClient, main: MainScreenModel) {
        self.presenter = presenter
        self.devices = devices
        self.main = main
        take = main.take
    }

    var body: some View {
        VStack(spacing: 0) {
            ConfirmationLayer(devices: devices, modeLabel: main.modeLabel, kindOf: main.foreign.kind(of:), take: take)
                .frame(height: presenter.coveredHeight)
            Spacer(minLength: 0)
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .top)
        .ignoresSafeArea()
        .animation(.easeOut(duration: 0.2), value: devices.question?.id)
        .animation(.easeOut(duration: 0.2), value: take.asked?.id)
    }
}
