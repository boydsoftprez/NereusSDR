#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// src/core/security/OpenSslErrorScope.h  (NereusSDR)
// =================================================================
//
// Leaves this thread's OpenSSL error queue empty when the scope ends, on
// every path out of it, a thrown exception included.
//
// OpenSSL keeps one error queue per thread. A failed call (a key whose
// point is not on the curve, a signature with r = 0, a PEM file with
// nothing after its certificate) pushes an error there and leaves it. Qt's
// OpenSSL TLS backend reads the same queue after its own calls on the same
// thread and takes a stale entry as its own failure, which ends a healthy
// wss:// connection: the Core's connection to the remote access service,
// or a live remote session (iPhone app plan Task 28, found by the traversal
// harness on Linux; the safety review's Important 3).
//
// Every NereusSDR entry point that calls OpenSSL on a thread Qt's TLS runs
// on opens one of these first. CertificateStore reads its errors for its
// messages and clears them itself.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-26: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
// =================================================================

#include <openssl/err.h>

namespace NereusSDR {

class OpenSslErrorScope {
public:
    OpenSslErrorScope() = default;
    ~OpenSslErrorScope() { ERR_clear_error(); }

    OpenSslErrorScope(const OpenSslErrorScope&) = delete;
    OpenSslErrorScope& operator=(const OpenSslErrorScope&) = delete;
};

} // namespace NereusSDR
