// NereusSDR for iOS: bounded asynchronous CFNetwork PAC evaluation.
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import CFNetwork
import Foundation

/// Invalidation stops a pending source. CFNetwork does not promise to
/// interrupt JavaScript that has already entered its callback, so cap those
/// native threads if a broken PAC script fails to return.
private enum PACWorkerSlots {
    static let lock = NSLock()
    nonisolated(unsafe) static var occupied = 0
    static let maximum = 2

    static func acquire() -> Bool {
        lock.withLock {
            guard occupied < maximum else { return false }
            occupied += 1
            return true
        }
    }

    static func release() {
        lock.withLock { occupied -= 1 }
    }
}

struct CFNetworkPACEvaluator: SystemProxyPACEvaluating {
    func evaluate(_ source: SystemProxyPACSource, for target: URL, timeout: Duration) async throws -> SystemProxyPACEntries {
        let operation = PACOperation(source: source, target: target)
        return try await withTaskCancellationHandler {
            try await withCheckedThrowingContinuation { continuation in
                operation.start(continuation: continuation, timeout: timeout)
            }
        } onCancel: {
            operation.finish(.failure(.cancelled))
        }
    }
}

/// CFNetwork PAC sources need a live run loop. The callback executes on this
/// operation's dedicated thread, never on Swift's cooperative executor.
final class PACOperation: @unchecked Sendable {
    private let source: SystemProxyPACSource
    private let target: URL
    private let lock = NSLock()
    private var continuation: CheckedContinuation<SystemProxyPACEntries, Error>?
    private var outcome: Result<SystemProxyPACEntries, SystemProxyError>?
    private var runLoop: CFRunLoop?
    private var runLoopSource: CFRunLoopSource?

    init(source: SystemProxyPACSource, target: URL) {
        self.source = source
        self.target = target
    }

    func start(continuation: CheckedContinuation<SystemProxyPACEntries, Error>, timeout: Duration) {
        let pending = lock.withLock { () -> Result<SystemProxyPACEntries, SystemProxyError>? in
            if let outcome { return outcome }
            self.continuation = continuation
            return nil
        }
        if let pending {
            continuation.resume(with: pending.mapError { $0 as Error })
            return
        }
        let parts = timeout.components
        let seconds = Double(parts.seconds) + Double(parts.attoseconds) / 1e18
        guard seconds > 0 else {
            finish(.failure(.timedOut))
            return
        }
        guard PACWorkerSlots.acquire() else {
            finish(.failure(.pacFailed))
            return
        }
        Thread.detachNewThread { [self] in
            defer { PACWorkerSlots.release() }
            run()
        }
        DispatchQueue.global().asyncAfter(deadline: .now() + seconds) { [self] in
            finish(.failure(.timedOut))
        }
    }

    func finish(_ result: Result<SystemProxyPACEntries, SystemProxyError>) {
        let (continuation, source, loop) = lock.withLock {
            () -> (CheckedContinuation<SystemProxyPACEntries, Error>?, CFRunLoopSource?, CFRunLoop?) in
            guard outcome == nil else { return (nil, nil, nil) }
            outcome = result
            let continuation = self.continuation
            self.continuation = nil
            return (continuation, runLoopSource, runLoop)
        }
        continuation?.resume(with: result.mapError { $0 as Error })
        if let source { CFRunLoopSourceInvalidate(source) }
        if let loop { CFRunLoopWakeUp(loop) }
    }

    private func run() {
        guard !isFinished else { return }
        var context = CFStreamClientContext(
            version: 0,
            info: Unmanaged.passUnretained(self).toOpaque(),
            retain: nil, release: nil, copyDescription: nil)
        let callback: CFProxyAutoConfigurationResultCallback = { info, proxies, error in
            let operation = Unmanaged<PACOperation>.fromOpaque(info).takeUnretainedValue()
            if error != nil {
                operation.finish(.failure(.pacFailed))
            } else {
                let dictionaries = (proxies as NSArray).compactMap { $0 as? NSDictionary }
                operation.finish(.success(SystemProxyPACEntries(entries: dictionaries)))
            }
        }
        let pacSource: CFRunLoopSource
        switch source {
        case .script(let script):
            pacSource = CFNetworkExecuteProxyAutoConfigurationScript(
                script as CFString, target as CFURL, callback, &context)
        case .url(let url):
            pacSource = CFNetworkExecuteProxyAutoConfigurationURL(
                url as CFURL, target as CFURL, callback, &context)
        }
        let loop = CFRunLoopGetCurrent()
        let shouldSchedule = lock.withLock { () -> Bool in
            if outcome != nil { return false }
            runLoop = loop
            runLoopSource = pacSource
            return true
        }
        guard shouldSchedule else {
            CFRunLoopSourceInvalidate(pacSource)
            return
        }
        CFRunLoopAddSource(loop, pacSource, CFRunLoopMode.defaultMode)
        while !isFinished {
            CFRunLoopRunInMode(CFRunLoopMode.defaultMode, 0.05, true)
        }
        CFRunLoopSourceInvalidate(pacSource)
        CFRunLoopRemoveSource(loop, pacSource, CFRunLoopMode.defaultMode)
        lock.withLock {
            runLoopSource = nil
            runLoop = nil
        }
    }

    private var isFinished: Bool { lock.withLock { outcome != nil } }
}
