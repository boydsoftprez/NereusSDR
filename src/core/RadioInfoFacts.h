#pragma once
// no-port-check: NereusSDR-original. The Radio Info tab's text, shared by
// the desktop tab and the Core's Setup description (R-R3-49, R-IOS-18).

#include "core/BoardCapabilities.h"
#include "core/HpsdrModel.h"
#include "core/RadioDiscovery.h"

#include <QString>

namespace NereusSDR {

// What Setup > Hardware Config > Radio Info shows for one radio, exactly as
// the tab shows it: a dash (U+2014) for a value the radio has not reported.
struct RadioInfoFacts {
    QString board;
    QString protocol;
    QString adcCount;
    QString maxRx;
    QString firmware;
    QString mac;
    QString ip;
    int maxSampleRateHz = 0;  // the protocol's top rate for this board
    /// The text Copy Support Info to Clipboard puts on the clipboard.
    QString supportText() const;
};

RadioInfoFacts radioInfoFacts(const RadioInfo& info, const BoardCapabilities& caps,
                              HPSDRModel model);

} // namespace NereusSDR
