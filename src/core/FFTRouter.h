// =================================================================
// src/core/FFTRouter.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. Routes FFT frames from receivers
// (DDCs) to subscribed pans per the AetherSDR overlay model (one DDC
// can feed N pans at different zoom levels of the same I/Q data).
// AetherSDR is a thin Flex API client; NereusSDR owns the FFT pipeline
// locally, so there is no upstream class to port. See
// docs/architecture/2026-05-26-phase3f-multi-pan-multi-slice-design.md
// for the design context.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-27  J.J. Boyd / KG4VCF  Phase 3F Sub-Epic D Task 7.
//                                    NereusSDR-original receiver-to-pan
//                                    fan-out router. Owns the bidirectional
//                                    receiverId <-> panId topology; the
//                                    RadioModel-level fan-out wiring lands
//                                    in Sub-Epic D Task 12+ when the
//                                    per-receiver FFTEngine pump is fed
//                                    through onFftFrame. AI-assisted
//                                    transformation via Anthropic Claude
//                                    Code.
// =================================================================
#pragma once

#include <QObject>
#include <QString>
#include <QMap>
#include <QList>
#include <QVector>

namespace NereusSDR {

/// Routes FFT frames from receivers to subscribed pans.
/// One receiver (DDC) -> 1..N pans (different zoom levels of same data).
/// See docs/architecture/2026-05-26-phase3f-multi-pan-multi-slice-design.md
/// §11.
class FFTRouter : public QObject {
    Q_OBJECT
public:
    explicit FFTRouter(QObject* parent = nullptr) : QObject(parent) {}

    void mapPanToReceiver(const QString& panId, int receiverId);
    void removePan(const QString& panId);
    void removeReceiver(int receiverId);

    QList<QString> pansForReceiver(int receiverId) const;
    QList<int> receiversForPan(const QString& panId) const;

public slots:
    /// Called by the per-receiver FFTEngine when a frame is ready. Fans out
    /// to every pan currently subscribed to that receiver.
    void onFftFrame(int receiverId, const QVector<float>& binsDbm);

signals:
    void fftFrameForPan(const QString& panId, int receiverId, const QVector<float>& binsDbm);

private:
    QMap<int, QList<QString>> m_receiverToPans;
};

} // namespace NereusSDR
