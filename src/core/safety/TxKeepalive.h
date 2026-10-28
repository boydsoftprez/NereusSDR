// no-port-check: NereusSDR-original remote transmit lease value.
// Modification history (NereusSDR):
//   2026-10-27: CW epoch extension by J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via OpenAI Codex.
#pragma once
#include <QtGlobal>
namespace NereusSDR {
struct TxKeepalive {
    quint64 sequence{0};
    quint32 epoch{0};
    quint32 cwEpoch{0};
};
} // namespace NereusSDR
