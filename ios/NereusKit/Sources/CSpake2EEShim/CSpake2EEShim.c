// NereusSDR for iOS: checks at build time that spake2-ee's sizes are the ones the link carries
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

#include "CSpake2EEShim.h"

/* Link document section 3.6: step 0 is 36 bytes, step 1 32, step 2 64,
   step 3 32, and each shared key 32, the key of the confirmation boxes. */
_Static_assert(crypto_spake_PUBLICDATABYTES == 36, "step 0 is 36 bytes");
_Static_assert(crypto_spake_RESPONSE1BYTES == 32, "step 1 is 32 bytes");
_Static_assert(crypto_spake_RESPONSE2BYTES == 64, "step 2 is 64 bytes");
_Static_assert(crypto_spake_RESPONSE3BYTES == 32, "step 3 is 32 bytes");
_Static_assert(crypto_spake_SHAREDKEYBYTES == crypto_aead_xchacha20poly1305_ietf_KEYBYTES,
               "a shared key is a box key");
