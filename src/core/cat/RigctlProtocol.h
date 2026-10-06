// Ported from AetherSDR src/core/RigctlProtocol.h [@1e0718ad].
// AetherSDR project attribution: Jeremy (KK7GWY), primary author, and contributors.
// https://github.com/ten9876/AetherSDR — GNU GPL v3, project LICENSE applies.
// Upstream source has no top-of-file GPL header; no notice is fabricated.
// Modification history (NereusSDR):
// 2026-10-04 - Slice-bound wire contracts adapted by J.J. Boyd (KG4VCF),
//              AI-assisted via OpenAI Codex. Native lifecycle/authority guards.
#pragma once
#include "CatTypes.h"
#include <QPointer>
#include <QStringList>
namespace NereusSDR {
class CatModelAdapter; class CatTxCoordinator; class CatService; class RadioModel;
// Pure protocol handler for Hamlib rigctld emulation.
// No I/O — receives a text line, returns the response string.
// Shared by both the TCP server and the PTY virtual serial port.
// [original inline comment from RigctlProtocol.h:14-16]
class RigctlProtocol {
public:
    RigctlProtocol(CatModelAdapter&, CatTxCoordinator&, int channel, quint64 sessionId);
    QString handleLine(const QString&);
    void reset() { m_vfo = CatVfo::Primary; }
private:
    bool live() const;
    int writeError(CatVfo, const QByteArray&) const;
    CatModelAdapter& m_adapter;
    CatTxCoordinator& m_coordinator;
    QPointer<RadioModel> m_model;
    QPointer<CatService> m_service;
    CatBinding m_binding;
    quint64 m_sessionId;
    CatVfo m_vfo{CatVfo::Primary};
};
} // namespace NereusSDR
