// =================================================================
// src/core/session/media/DisplayBudgetSplit.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. See DisplayBudgetSplit.h.
//
// =================================================================

#include "core/session/media/DisplayBudgetSplit.h"

#include <algorithm>
#include <numeric>
#include <vector>

namespace NereusSDR {
namespace {

// Max-min fair shares of `available` for the given requests, in input
// order. Devices are served smallest request first; each takes the lesser
// of its request and an equal part of what remains, so what a small
// request leaves goes to the rest. The integer remainder falls to the
// device served last (the largest request, the latest in input order among
// equal ones), so the shares sum to `available` whenever the requests reach
// it.
std::vector<quint64> maxMinFair(const std::vector<quint64>& requests, quint64 available)
{
    std::vector<quint64> shares(requests.size(), 0);
    std::vector<std::size_t> order(requests.size());
    std::iota(order.begin(), order.end(), std::size_t{0});
    std::stable_sort(order.begin(), order.end(), [&requests](std::size_t a, std::size_t b) {
        return requests[a] < requests[b];
    });
    quint64 remaining = available;
    std::size_t left = order.size();
    for (std::size_t index : order) {
        const quint64 equal = remaining / left;
        const quint64 give = left == 1 ? std::min(requests[index], remaining)
                                       : std::min(requests[index], equal);
        shares[index] = give;
        remaining -= give;
        --left;
    }
    return shares;
}

struct Dimension {
    quint64 total = 0;
    quint64 ps3 = 0;
    std::vector<quint64> requests;
};

// The shares of one dimension, before the floor of 1, and the part of each
// share that answers the device's own request (without PureSignal's).
std::vector<quint64> splitDimension(const Dimension& d, std::optional<std::size_t> holder,
                                    std::optional<std::size_t> ps3Subscriber)
{
    const std::size_t count = d.requests.size();
    std::vector<quint64> shares(count, 0);
    // Rule 4: PureSignal's display comes off the top, once.
    const quint64 ps3 = ps3Subscriber ? std::min(d.ps3, d.total) : 0;
    quint64 remaining = d.total - ps3;
    // Rule 1: a present network holder takes its whole request.
    if (holder) {
        shares[*holder] = std::min(d.requests[*holder], remaining);
        remaining -= shares[*holder];
    }
    // Rules 2 and 3: the rest, max-min fair among the others.
    std::vector<quint64> otherRequests;
    std::vector<std::size_t> others;
    for (std::size_t i = 0; i < count; ++i) {
        if (holder && *holder == i) {
            continue;
        }
        others.push_back(i);
        otherRequests.push_back(d.requests[i]);
    }
    const std::vector<quint64> fair = maxMinFair(otherRequests, remaining);
    quint64 given = 0;
    for (std::size_t k = 0; k < others.size(); ++k) {
        shares[others[k]] = fair[k];
        given += fair[k];
    }
    remaining -= given;
    // Fix wave I5: what no device asks for is shared equally among all of
    // them as room to grow into (the remainder to the last), so a device
    // alone keeps the whole total and a device may ask for more than it
    // has without another device being cut below its own request.
    if (remaining > 0 && count > 0) {
        const quint64 each = remaining / count;
        for (std::size_t i = 0; i < count; ++i) {
            shares[i] += each;
        }
        shares[count - 1] += remaining - each * count;
    }
    return shares;
}

quint32 nextGeneration(const std::optional<DisplayBudgetLimits>& previous, quint32 total)
{
    if (!previous) {
        return total;
    }
    if (DisplayBudgetSplit::newerGeneration(total, previous->generation)) {
        return total;
    }
    quint32 next = previous->generation + 1;
    if (next == 0) {
        ++next;
    }
    return next;
}

} // namespace

bool DisplayBudgetSplit::newerGeneration(quint32 candidate, quint32 reference)
{
    const quint32 difference = candidate - reference;
    return difference != 0 && difference < 0x80000000u;
}

QList<DisplayBudgetShare> DisplayBudgetSplit::split(const DisplayBudgetSplitInput& input)
{
    QList<DisplayBudgetShare> result;
    const std::size_t count = static_cast<std::size_t>(input.devices.size());
    if (count == 0 || !input.total.isValid()) {
        return result;
    }

    std::optional<std::size_t> holder;
    std::optional<std::size_t> ps3Subscriber;
    for (std::size_t i = 0; i < count; ++i) {
        const QByteArray& id = input.devices.at(static_cast<qsizetype>(i)).id;
        if (input.holderKind == DisplayBudgetHolderKind::Device && !input.holderAway
            && id == input.holderId) {
            holder = i;
        }
        if (input.ps3Subscriber && id == *input.ps3Subscriber) {
            ps3Subscriber = i;
        }
    }

    const DisplayBudgetCharge ps3 = ps3DisplayCharge();
    Dimension bytes{input.total.applicationBytesPerSecond, ps3.applicationBytesPerSecond, {}};
    Dimension samples{input.total.spectrumSampleUnitsPerSecond,
                      ps3.spectrumSampleUnitsPerSecond, {}};
    // Fix wave 2 (Critical 1): every device counts as asking for at least
    // one useful pan.
    for (const DisplayBudgetSplitDevice& device : input.devices) {
        bytes.requests.push_back(std::max(device.request.applicationBytesPerSecond,
                                          input.minimumRequest.applicationBytesPerSecond));
        samples.requests.push_back(std::max(device.request.spectrumSampleUnitsPerSecond,
                                            input.minimumRequest.spectrumSampleUnitsPerSecond));
    }
    const std::vector<quint64> byteShares = splitDimension(bytes, holder, ps3Subscriber);
    const std::vector<quint64> sampleShares = splitDimension(samples, holder, ps3Subscriber);

    const DisplayBudgetReason alone = input.governorCut ? DisplayBudgetReason::CoreBusy
                                                        : DisplayBudgetReason::None;
    const DisplayBudgetReason shared = input.governorCut
        ? DisplayBudgetReason::SharedProcessing : DisplayBudgetReason::SharedConnection;
    for (std::size_t i = 0; i < count; ++i) {
        const DisplayBudgetSplitDevice& device = input.devices.at(static_cast<qsizetype>(i));
        const bool below = byteShares[i] < bytes.requests[i]
            || sampleShares[i] < samples.requests[i];
        DisplayBudgetShare share;
        share.id = device.id;
        share.reason = count > 1 && below ? shared : alone;
        quint64 shareBytes = byteShares[i];
        quint64 shareSamples = sampleShares[i];
        if (ps3Subscriber && *ps3Subscriber == i) {
            shareBytes += std::min(bytes.ps3, bytes.total);
            shareSamples += std::min(samples.ps3, samples.total);
        }
        share.limits.applicationBytesPerSecond = std::max<quint64>(1, shareBytes);
        share.limits.spectrumSampleUnitsPerSecond = std::max<quint64>(1, shareSamples);
        const bool unchanged = device.previous && device.previousReason
            && device.previous->applicationBytesPerSecond == share.limits.applicationBytesPerSecond
            && device.previous->spectrumSampleUnitsPerSecond
                == share.limits.spectrumSampleUnitsPerSecond
            && *device.previousReason == share.reason;
        share.limits.generation = unchanged ? device.previous->generation
                                            : nextGeneration(device.previous,
                                                             input.total.generation);
        result.append(share);
    }
    return result;
}

} // namespace NereusSDR
