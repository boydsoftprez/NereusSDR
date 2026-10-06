// NereusSDR for iOS: spake2-ee's header for Swift, with the declarations it needs included first
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

#ifndef NEREUS_CSPAKE2EE_SHIM_H
#define NEREUS_CSPAKE2EE_SHIM_H

/* crypto_spake.h uses size_t and includes nothing itself. The vendored
   copy stays unchanged, so the declarations it needs come first here. */
#include <stddef.h>
#include <sodium.h>

#include <crypto_spake.h>

#endif
