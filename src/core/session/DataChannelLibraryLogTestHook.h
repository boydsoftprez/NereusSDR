#pragma once
// =================================================================
// src/core/session/DataChannelLibraryLogTestHook.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original.
//
// Test builds only (NEREUS_BUILD_TESTS): the libdatachannel / libjuice log
// hook the transport tests use to see which thread each library line comes
// from (R-R3-49). It lived on DataChannelTransport as a static test seam;
// it is here so product code carries no test entry point. It sits in the
// library, not in the tests, because the library links libdatachannel
// statically: a test that called rtc::InitLogger from its own copy would
// configure a logger the transport never uses.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-28: moved out of DataChannelTransport, no behavior change.
//               J.J. Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
// =================================================================

#ifdef NEREUS_BUILD_TESTS

#include <QString>
#include <QtGlobal>

#include <functional>

namespace NereusSDR::testhooks {

/// Each line libdatachannel and libjuice log, at every level, goes to `sink`
/// with the logging thread's id, on that thread, until the sink is set empty
/// (logging off, as it is by default). Process-wide. The sink must not block
/// or call into the library: the library holds its log lock while it runs.
void setDataChannelLibraryLog(
    std::function<void(quintptr thread, const QString& line)> sink);

} // namespace NereusSDR::testhooks

#endif // NEREUS_BUILD_TESTS
