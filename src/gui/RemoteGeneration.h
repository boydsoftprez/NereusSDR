// =================================================================
// src/gui/RemoteGeneration.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  One wrap-aware generation test shared
// by RemoteMediaController and RemoteAudioStatus.
// =================================================================
#pragma once

#include <QtGlobal>

namespace NereusSDR {

// True when `next` is a later context generation than `previous`: different,
// and less than half the 32-bit range ahead, so a counter that wraps past
// 0xFFFFFFFF still counts as newer.
inline bool isNewerGeneration(quint32 next, quint32 previous)
{
    return next != previous && quint32(next - previous) < 0x80000000u;
}

} // namespace NereusSDR
