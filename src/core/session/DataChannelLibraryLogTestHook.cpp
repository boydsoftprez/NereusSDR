// =================================================================
// src/core/session/DataChannelLibraryLogTestHook.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original.
//
// See DataChannelLibraryLogTestHook.h. Compiles to nothing unless
// NEREUS_BUILD_TESTS is defined.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-28: moved out of DataChannelTransport, no behavior change.
//               J.J. Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
// =================================================================

#include "core/session/DataChannelLibraryLogTestHook.h"

#ifdef NEREUS_BUILD_TESTS

#include <QThread>

#include <rtc/rtc.hpp>

#include <string>
#include <utility>

namespace NereusSDR::testhooks {

void setDataChannelLibraryLog(
    std::function<void(quintptr thread, const QString& line)> sink)
{
    if (!sink) {
        rtc::InitLogger(rtc::LogLevel::None);
        return;
    }
    rtc::InitLogger(rtc::LogLevel::Verbose,
                    [sink = std::move(sink)](rtc::LogLevel, const std::string& line) {
        sink(reinterpret_cast<quintptr>(QThread::currentThreadId()),
             QString::fromStdString(line));
    });
}

} // namespace NereusSDR::testhooks

#endif // NEREUS_BUILD_TESTS
