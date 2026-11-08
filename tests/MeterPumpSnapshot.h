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
#include <QtGlobal>

namespace NereusSDR::Test {

// Polls until `snapshot` and what the pump wrote to `slice` come from one lane
// refresh. A poll's own refresh rarely lands in the moment before the read,
// and QTest::qWaitFor gives the lane time to run it before the next attempt.
// False if no attempt matched within timeoutMs: the slice's stamp never named
// the refresh the channel holds, and the last two stamps are printed. A clear
// that changes a reading stamps with the clock, not a refresh, so it does not
// match. A clear that changes nothing keeps the slice's last stamp, and that
// can still match, so this alone does not say the slice holds a reading: the
// caller that needs one checks its values. Nor does a match say the refresh
// is newer than some event. A poll that returns early on a lost link asks for
// no refresh, so the first poll after it publishes the cache from before the
// loss; a caller that must rule that out compares meterReadingNs with the
// clear's stamp.
[[nodiscard]] inline bool pollUntilPumpWroteSnapshot(SliceMeterPump& pump, const RxChannel& channel,
                                                     const SliceModel& slice,
                                                     RxChannel::MeterReadings& snapshot,
                                                     int timeoutMs = kNereusTryDefaultMs)
{
    const bool matched = QTest::qWaitFor(
        [&]() {
            pump.poll();
            snapshot = channel.meterReadings();
            return snapshot.readNs > 0 && snapshot.readNs == slice.meterReadingNs();
        },
        timeoutMs);
    if (!matched) {
        qWarning("pollUntilPumpWroteSnapshot: no match in %d ms: slice meterReadingNs %lld, "
                 "channel readNs %lld, meterReadingReady %d, slice ADC peak %f",
                 timeoutMs, static_cast<long long>(slice.meterReadingNs()),
                 static_cast<long long>(snapshot.readNs), channel.meterReadingReady() ? 1 : 0,
                 slice.adcPeakDbfs());
    }
    return matched;
}

} // namespace NereusSDR::Test
