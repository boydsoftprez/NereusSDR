// no-port-check: NereusSDR-original test helper. It calls the production
// accessors SliceMeterPump calls; no Thetis logic is ported here.
// =================================================================
// tests/MeterPumpSnapshot.h  (NereusSDR)
// =================================================================
//
// Comparing what SliceMeterPump wrote with what the channel holds.
//
// The pump does not read WDSP. poll() publishes the receive lane's meter
// cache and asks the lane for the next refresh (RxChannel::meterReadings), so
// what it writes is one refresh old by design, and the slice's meterReadingNs
// says which refresh. On a channel that is receiving, each WDSP block moves
// the meters, so a later read of the channel (getMeter, or meterReadings
// again) may rightly differ from the slice if that refresh landed in between:
// the pump was right and the comparison was wrong. PR 757's CI saw it as ADC
// peak -58.295 on the slice against -41.249 on the channel, two blocks apart.
//
// So a test holds a read to the refresh the pump published: poll, read, and
// accept the pair only when their stamps match. The values must then be
// identical, and a pump that wrote stale readings, or none, never gets there.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-06 - Written for NereusSDR by J.J. Boyd (KG4VCF), with
//                AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#pragma once

#include "SessionWait.h"

#include "core/RxChannel.h"
#include "core/meters/SliceMeterPump.h"
#include "models/SliceModel.h"

#include <QTest>

namespace NereusSDR::Test {

// Polls until `snapshot` and what the pump wrote to `slice` come from one lane
// refresh. A poll's own refresh rarely lands in the moment before the read,
// and QTest::qWaitFor gives the lane time to run it before the next attempt.
// False if no attempt matched within timeoutMs: the slice's stamp never named
// the refresh the channel holds. A clear stamps with the clock, not a refresh,
// so a slice with no reading never matches.
[[nodiscard]] inline bool pollUntilPumpWroteSnapshot(SliceMeterPump& pump, const RxChannel& channel,
                                                     const SliceModel& slice,
                                                     RxChannel::MeterReadings& snapshot,
                                                     int timeoutMs = kNereusTryDefaultMs)
{
    return QTest::qWaitFor(
        [&]() {
            pump.poll();
            snapshot = channel.meterReadings();
            return snapshot.readNs > 0 && snapshot.readNs == slice.meterReadingNs();
        },
        timeoutMs);
}

} // namespace NereusSDR::Test
