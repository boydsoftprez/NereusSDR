// no-port-check: NereusSDR-original test helper.
//
// RealtimeTestLoad: one log line with the machine's load average, printed
// when a `realtime` test case fails (R-R3-21, R-R3-40). A `realtime` test
// measures real-time behaviour against the wall clock, so other programs
// loading the machine can fail it; the line shows in the log whether that
// was the case. Call it from the test class's cleanup() slot, which QtTest
// runs after every case and data row, failed or not.
//
// See docs/development/fast-test-loop.md, "Real-time tests".
#pragma once

#include <QtTest>

#if !defined(Q_OS_WIN)
#include <stdlib.h>
#endif

namespace NereusSDR::RealtimeTestLoad {

/// Prints the 1, 5 and 15 minute load averages when the current case failed.
inline void printLoadAverageIfFailed()
{
    if (!QTest::currentTestFailed()) {
        return;
    }
    const char* testFunction = QTest::currentTestFunction();
    const char* dataTag = QTest::currentDataTag();
    const QByteArray where = dataTag
        ? QByteArray(testFunction) + '(' + dataTag + ')'
        : QByteArray(testFunction);
#if defined(Q_OS_WIN)
    qInfo("realtime test %s failed; load average not available on Windows",
          where.constData());
#else
    double load[3] = {0.0, 0.0, 0.0};
    if (getloadavg(load, 3) == 3) {
        qInfo("realtime test %s failed at load average %.2f %.2f %.2f (1, 5, 15 min)",
              where.constData(), load[0], load[1], load[2]);
    } else {
        qInfo("realtime test %s failed; load average could not be read",
              where.constData());
    }
#endif
}

} // namespace NereusSDR::RealtimeTestLoad
