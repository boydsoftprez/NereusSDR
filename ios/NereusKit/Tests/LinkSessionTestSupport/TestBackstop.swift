// NereusSDR for iOS: how long a test waits in real time before it calls a missing step a hang
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

/// How long a test waits in real time for something that arrives as soon
/// as its task runs, before it fails instead of hanging. The timing a test
/// checks is on its own clock, moved by hand; this bound only stops a
/// broken test from waiting for ever.
///
/// It is sized for the test process, not for the code under test. CI runs
/// all 1,730 package tests in one process on a three-core runner, and they
/// all start at once: about 40 core-seconds of synchronous work in that
/// first wave kept every task in the process from running for 13 s
/// (2026-10-09, PR 367's run 37891531574), so a 10 s bound that started
/// with it ran out with nothing wrong. A wait that is expected to end
/// with nothing, such as "no second message within 1 s", keeps its own
/// short bound.
public enum TestBackstop {
    public static let hang: Duration = .seconds(60)
}
