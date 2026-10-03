#pragma once
// tests/SessionWait.h
//
// no-port-check: NereusSDR-original test helper.
//
// Waits for the session tests (tst_station_session, tst_remote_peripherals).
//
// Positive checks: QTRY_ polls in whole 50 ms sleeps (qtestcase.h QTRY_IMPL
// steps by 50 ms at its 5 s default), so every wait not already true costs a
// full step whether the answer landed after 1 ms or 49; a case with dozens
// of them spends most of its time asleep, and a loaded machine stretches the
// run past ctest's limit. NEREUS_TRY_* wait on the same condition with the
// same 5 s default through QTest::qWaitFor, which checks it after every
// event-loop pass, and then assert it exactly as QVERIFY / QCOMPARE do.
//
// "Nothing happens" checks on a window-to-Core link: a check that something
// did NOT happen has nothing to wait for, so it waits for the product's own
// bound instead of a number picked by hand: an echo, a stray write or a
// refused command would be on its way within two ticks of either side's
// coalescing timer (StationClient::kDefaultWriteFlushMs toward the Core,
// StationServer::kDefaultDeltaFlushMs toward the window), and then needs
// only the queued deliveries the loopback transport and the local sockets
// hand over on later event-loop turns.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-29  J.J. Boyd / KG4VCF  Load finding: waits that wake on the
//                                    answer, and one named wait for the
//                                    session tests' nothing-happens checks.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  NEREUS_TRY_COMPARE waits on floating
//                                    point values the way QCOMPARE compares
//                                    them (qFuzzyCompare, and qFuzzyIsNull
//                                    near zero), so a wait no longer runs
//                                    to its timeout on a value QCOMPARE
//                                    passes. AI-assisted via Anthropic
//                                    Claude Code.
// =================================================================

#include <QCoreApplication>
#include <QTest>
#include <QtNumeric>

#include <algorithm>
#include <cmath>
#include <type_traits>

#include "core/session/StationClient.h"
#include "core/session/StationServer.h"

namespace NereusSDR::Test {

/// Whether QCOMPARE(actual, expected) would pass: floating point values
/// compare as QCOMPARE compares them (infinities by sign, NaN with NaN,
/// qFuzzyCompare, and qFuzzyIsNull when the expected value is zero or
/// subnormal); everything else with ==.
template <typename Actual, typename Expected>
bool comparesAsQCompare(const Actual& actual, const Expected& expected)
{
    if constexpr (std::is_floating_point_v<Actual> && std::is_floating_point_v<Expected>) {
        using Value = std::common_type_t<Actual, Expected>;
        const Value a = static_cast<Value>(actual);
        const Value e = static_cast<Value>(expected);
        switch (std::fpclassify(e)) {
        case FP_INFINITE:
            return std::isinf(a) && std::signbit(a) == std::signbit(e);
        case FP_NAN:
            return std::isnan(a);
        case FP_ZERO:
        case FP_SUBNORMAL:
            return qFuzzyIsNull(a);
        default:
            return qFuzzyIsNull(e) ? qFuzzyIsNull(a) : qFuzzyCompare(a, e);
        }
    } else {
        return static_cast<bool>(actual == expected);
    }
}

#define NEREUS_TRY_VERIFY_WITH_TIMEOUT(expr, timeoutMs)                                   \
    do {                                                                                  \
        (void)QTest::qWaitFor([&]() { return static_cast<bool>(expr); }, (timeoutMs));    \
        QVERIFY(expr);                                                                    \
    } while (false)

#define NEREUS_TRY_VERIFY(expr) NEREUS_TRY_VERIFY_WITH_TIMEOUT(expr, 5000)

#define NEREUS_TRY_VERIFY2(expr, message)                                                 \
    do {                                                                                  \
        (void)QTest::qWaitFor([&]() { return static_cast<bool>(expr); }, 5000);           \
        QVERIFY2(expr, message);                                                          \
    } while (false)

#define NEREUS_TRY_COMPARE_WITH_TIMEOUT(actual, expected, timeoutMs)                      \
    do {                                                                                  \
        (void)QTest::qWaitFor(                                                            \
            [&]() { return NereusSDR::Test::comparesAsQCompare((actual), (expected)); },  \
            (timeoutMs));                                                                 \
        QCOMPARE(actual, expected);                                                       \
    } while (false)

#define NEREUS_TRY_COMPARE(actual, expected) NEREUS_TRY_COMPARE_WITH_TIMEOUT(actual, expected, 5000)

/// Runs what is already queued, and what that queues in turn, to a depth
/// no link exchange here exceeds.
inline void drainQueuedDeliveries()
{
    for (int turn = 0; turn < 8; ++turn) {
        QCoreApplication::sendPostedEvents();
        QCoreApplication::processEvents();
    }
}

/// Two ticks of the slower coalescing timer on either side of the link.
inline constexpr int kSessionSettleMs =
    2 * std::max(NereusSDR::StationClient::kDefaultWriteFlushMs,
                 NereusSDR::StationServer::kDefaultDeltaFlushMs);

/// Waits out anything the link could still deliver, then runs it.
inline void settleSession()
{
    drainQueuedDeliveries();
    QTest::qWait(kSessionSettleMs);
    drainQueuedDeliveries();
}

} // namespace NereusSDR::Test
