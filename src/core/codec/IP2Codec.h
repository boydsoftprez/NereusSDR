// =================================================================
// src/core/codec/IP2Codec.h  (NereusSDR)
// =================================================================
//
// Per-board codec interface for the Protocol 2 command-packet compose
// layer. Subclasses model board variants:
// P2CodecOrionMkII handles the Orion-MKII / 7000D / 8000D / AnvelinaPro3
// family; P2CodecSaturn extends it with the G8NJJ Saturn BPF1 band-edge
// override for ANAN-G2 / G2-1K.
//
// P2RadioConnection owns std::unique_ptr<IP2Codec> chosen at connect time
// from m_hardwareProfile.model (see applyBoardQuirks()).
//
// Parallel to IP1Codec (which composes 5-byte C&C banks for Protocol 1);
// P2 uses four fixed-size command packets instead — CmdGeneral (60),
// CmdHighPriority (1444), CmdRx (1444), CmdTx (60).
//
// NereusSDR-original. Independently implemented from IP1Codec.h interface.
// No Thetis port; no PROVENANCE row.
// =================================================================

#pragma once

#include <QtGlobal>
#include "CodecContext.h"

namespace NereusSDR {

class IP2Codec {
public:
    virtual ~IP2Codec() = default;

    // Each composer fills the buffer with the post-Phase-B byte layout.
    // Buffers are caller-allocated and zero-initialized by the caller.
    // Sequence numbers (bytes 0-3 of each packet) are NOT stamped here —
    // the caller (P2RadioConnection::sendCmd*) stamps them just before
    // UDP transmission so they remain monotonic across retransmits.
    virtual void composeCmdGeneral     (const CodecContext& ctx, quint8 buf[60])   const = 0;
    virtual void composeCmdHighPriority(const CodecContext& ctx, quint8 buf[1444]) const = 0;
    virtual void composeCmdRx          (const CodecContext& ctx, quint8 buf[1444]) const = 0;
    virtual void composeCmdTx          (const CodecContext& ctx, quint8 buf[60])   const = 0;
};

} // namespace NereusSDR
