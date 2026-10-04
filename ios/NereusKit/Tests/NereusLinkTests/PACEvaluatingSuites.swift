// NereusSDR for iOS: one serialized home for the test suites that evaluate PAC scripts
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

import Testing

/// The suites that run real CFNetwork PAC evaluations. The app lets two PAC
/// evaluations run at once, process-wide, and refuses a third at once with
/// `.pacFailed`; the test process is one process, so suites evaluating PAC
/// in parallel would take each other's places and fail a correct test.
/// Nested here, they run one test at a time.
@Suite(.serialized) enum PACEvaluatingSuites {}
